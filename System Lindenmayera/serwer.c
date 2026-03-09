// server_handover_v11_retx.c
// v11: v10-interactive + retransmission (stop-and-wait) if STEP_DONE doesn't arrive
// - UDP protocol unchanged
// - HANDOVER_REQUEST uses next= and state= from node (robust)
// - Server keeps stack for [ ]
// - Finish if turtle goes out of canvas (or next out of canvas), save output.txt
// - L-system from file argv[1] OR interactively from stdin
// - start from infile.txt: start=x:y[:a]

#include <arpa/inet.h>
#include <ctype.h>
#include <math.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define MAX_NODES 4
#define MAX_BUF 4096
#define MAX_RULES 16
#define MAX_LSYS_LEN 8192
#define NODE_LISTEN_PORT 5001
#define SERVER_STACK_SIZE 256

typedef struct {
  char key;
  char value[256];
} Rule;

typedef struct {
  int idx, x_min, y_min, x_max, y_max;
} Region;

typedef struct {
  char node_id[32];
  struct sockaddr_in addr;
  Region region;
  int done;
} Node;

typedef struct {
  int messages;
  int bytes_total;
  int handovers;
  int plots_total;
  int plots_unique;
} Metrics;

typedef struct {
  char axiom[512];
  Rule rules[MAX_RULES];
  int rule_count;
  int iterations;

  int canvas_w;
  int canvas_h;
  int step;
  int angle_step; // fixed 90
  int start_x;
  int start_y;
  int start_a;
} LsysConfig;

// -------------------- time --------------------
static long long now_ms(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (long long)tv.tv_sec * 1000LL + (tv.tv_usec / 1000LL);
}

// -------------------- metrics & send --------------------
static void record_metrics(Metrics *m, int size) {
  m->messages++;
  m->bytes_total += size;
}

static double avg_packet_size(const Metrics *m) {
  return m->messages ? (double)m->bytes_total / (double)m->messages : 0.0;
}

static void send_message(int sock, const struct sockaddr_in *addr, const char *payload, Metrics *metrics) {
  int len = (int)strlen(payload);
  (void)sendto(sock, payload, len, 0, (const struct sockaddr *)addr, sizeof(*addr));
  record_metrics(metrics, len);
}

// fields: "k=v;k=v;k=v"
static int parse_field(const char *fields, const char *key, char *out, size_t out_size) {
  const char *cursor = fields;
  size_t key_len = strlen(key);

  while (cursor && *cursor) {
    const char *eq = strchr(cursor, '=');
    if (!eq) return 0;

    size_t klen = (size_t)(eq - cursor);
    if (klen == key_len && strncmp(cursor, key, key_len) == 0) {
      const char *value_start = eq + 1;
      const char *end = strchr(value_start, ';');
      size_t vlen = end ? (size_t)(end - value_start) : strlen(value_start);
      if (vlen >= out_size) vlen = out_size - 1;
      memcpy(out, value_start, vlen);
      out[vlen] = '\0';
      return 1;
    }

    cursor = strchr(cursor, ';');
    if (cursor) cursor++;
  }
  return 0;
}

// -------------------- regions --------------------
static void compute_regions(Region *regions, int count, int width, int height) {
  int columns = (int)ceil(sqrt((double)count));
  int rows = (int)ceil((double)count / (double)columns);
  int region_width = width / columns;
  int region_height = height / rows;

  int idx = 0;
  for (int row = 0; row < rows; row++) {
    for (int col = 0; col < columns; col++) {
      if (idx >= count) break;
      Region r;
      r.idx = idx;
      r.x_min = col * region_width;
      r.y_min = row * region_height;
      r.x_max = (col == columns - 1) ? (width - 1) : (r.x_min + region_width - 1);
      r.y_max = (row == rows - 1) ? (height - 1) : (r.y_min + region_height - 1);
      regions[idx++] = r;
    }
  }
}

static int find_node_index(Node *nodes, int node_count, const char *node_id) {
  for (int i = 0; i < node_count; i++) {
    if (strcmp(nodes[i].node_id, node_id) == 0) return i;
  }
  return -1;
}

static int region_contains(const Region *r, int x, int y) {
  return x >= r->x_min && x <= r->x_max && y >= r->y_min && y <= r->y_max;
}

// -------------------- L-system derivation --------------------
static void derive_lsystem(const char *axiom, const Rule *rules, int rule_count, int iterations,
                          char *out, size_t out_size) {
  char a[MAX_LSYS_LEN], b[MAX_LSYS_LEN];
  strncpy(a, axiom, sizeof(a) - 1);
  a[sizeof(a) - 1] = '\0';

  char *cur = a;
  char *nxt = b;

  for (int it = 0; it < iterations; it++) {
    size_t out_len = 0;

    for (size_t i = 0; cur[i] != '\0'; i++) {
      char sym = cur[i];
      const char *rep = NULL;

      for (int r = 0; r < rule_count; r++) {
        if (rules[r].key == sym) {
          rep = rules[r].value;
          break;
        }
      }

      if (!rep) {
        if (out_len + 1 < MAX_LSYS_LEN) nxt[out_len++] = sym;
      } else {
        size_t rl = strlen(rep);
        if (out_len + rl < MAX_LSYS_LEN) {
          memcpy(nxt + out_len, rep, rl);
          out_len += rl;
        }
      }
    }

    nxt[out_len] = '\0';
    char *tmp = cur;
    cur = nxt;
    nxt = tmp;
  }

  strncpy(out, cur, out_size - 1);
  out[out_size - 1] = '\0';
}

// -------------------- canvas output --------------------
static void init_final_canvas(char final_canvas[40][81], int width, int height) {
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) final_canvas[y][x] = ' ';
    final_canvas[y][width] = '\0';
  }
}

static void write_output_txt(const char *path, char final_canvas[40][81], int height) {
  FILE *fp = fopen(path, "w");
  if (!fp) {
    perror("fopen(output.txt)");
    return;
  }
  for (int y = 0; y < height; y++) fprintf(fp, "%s\n", final_canvas[y]);
  fclose(fp);
  printf("[SERVER] Saved %s\n", path);
}

// -------------------- protocol sends --------------------
static void send_step(int sock, Node *nodes, int current_node_idx,
                      int current_index, char symbol, Metrics *metrics) {
  char msg[MAX_BUF];
  snprintf(msg, sizeof(msg), "RENDER_STEP|index=%d;symbol=%c", current_index, symbol);
  send_message(sock, &nodes[current_node_idx].addr, msg, metrics);
}

static void send_done_all(int sock, Node *nodes, int node_count, Metrics *metrics) {
  for (int i = 0; i < node_count; i++) {
    send_message(sock, &nodes[i].addr, "RENDER_DONE|id=server", metrics);
  }
}

// -------------------- turtle math (grid) --------------------
static void turtle_peek_next(int x, int y, int ang_deg, int step, int *nx, int *ny) {
  int a = ang_deg % 360;
  if (a < 0) a += 360;

  *nx = x;
  *ny = y;

  if (a == 0) (*nx) += step;
  else if (a == 90) (*ny) += step;
  else if (a == 180) (*nx) -= step;
  else if (a == 270) (*ny) -= step;
  else {
    double rad = (double)ang_deg * 3.141592653589793 / 180.0;
    *nx = (int)lround((double)x + cos(rad) * (double)step);
    *ny = (int)lround((double)y + sin(rad) * (double)step);
  }
}

// -------------------- finish control --------------------
static void begin_finish(int sock, Node *nodes, int node_count,
                         int *started, int *waiting_ack, int *finishing,
                         Metrics *metrics, const char *reason) {
  if (*finishing) return;
  *finishing = 1;
  *waiting_ack = 0;
  *started = 2;
  send_done_all(sock, nodes, node_count, metrics);
  printf("[SERVER] FINISH: %s -> sent RENDER_DONE to all nodes\n", reason);
}

// -------------------- input helpers --------------------
static void rstrip(char *s) {
  size_t n = strlen(s);
  while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || isspace((unsigned char)s[n - 1]))) {
    s[n - 1] = '\0';
    n--;
  }
}

static char *lstrip(char *s) {
  while (*s && isspace((unsigned char)*s)) s++;
  return s;
}

static int is_ignored_line(const char *s) {
  if (!s || !*s) return 1;
  if (s[0] == '#') return 1;
  if (s[0] == '/' && s[1] == '/') return 1;
  return 0;
}

// Accept rule lines: F,FF+F   or   F->FF+F
static int parse_rule_line(const char *line, Rule *out) {
  const char *comma = strchr(line, ',');
  if (comma && comma > line) {
    out->key = line[0];
    const char *rhs = comma + 1;
    while (*rhs && isspace((unsigned char)*rhs)) rhs++;
    strncpy(out->value, rhs, sizeof(out->value) - 1);
    out->value[sizeof(out->value) - 1] = '\0';
    return 1;
  }

  const char *arrow = strstr(line, "->");
  if (arrow && arrow > line) {
    out->key = line[0];
    const char *rhs = arrow + 2;
    while (*rhs && isspace((unsigned char)*rhs)) rhs++;
    strncpy(out->value, rhs, sizeof(out->value) - 1);
    out->value[sizeof(out->value) - 1] = '\0';
    return 1;
  }
  return 0;
}

// Format (ignores comments/blank):
// axiom
// rule_count
// rule lines
// iterations
static int load_lsys_config_file(const char *path, LsysConfig *cfg) {
  memset(cfg, 0, sizeof(*cfg));
  cfg->canvas_w = 80;
  cfg->canvas_h = 40;
  cfg->step = 1;
  cfg->angle_step = 90;
  cfg->start_x = cfg->canvas_w / 2;
  cfg->start_y = cfg->canvas_h / 2;
  cfg->start_a = 90;

  FILE *fp = fopen(path, "r");
  if (!fp) {
    perror("fopen(lsys file)");
    return 0;
  }

  char line[2048];
  int stage = 0;
  int rules_read = 0;

  while (fgets(line, sizeof(line), fp)) {
    rstrip(line);
    char *s = lstrip(line);
    if (is_ignored_line(s)) continue;

    if (stage == 0) {
      strncpy(cfg->axiom, s, sizeof(cfg->axiom) - 1);
      cfg->axiom[sizeof(cfg->axiom) - 1] = '\0';
      stage = 1;
      continue;
    }
    if (stage == 1) {
      cfg->rule_count = atoi(s);
      if (cfg->rule_count < 0 || cfg->rule_count > MAX_RULES) {
        printf("[SERVER] bad rule_count=%d (max %d)\n", cfg->rule_count, MAX_RULES);
        fclose(fp);
        return 0;
      }
      stage = 2;
      continue;
    }
    if (stage == 2) {
      if (rules_read < cfg->rule_count) {
        Rule r;
        if (!parse_rule_line(s, &r)) {
          printf("[SERVER] bad rule line: '%s'\n", s);
          fclose(fp);
          return 0;
        }
        cfg->rules[rules_read++] = r;
        if (rules_read == cfg->rule_count) stage = 3;
        continue;
      }
      stage = 3;
    }
    if (stage == 3) {
      cfg->iterations = atoi(s);
      fclose(fp);

      if (cfg->axiom[0] == '\0') {
        printf("[SERVER] missing axiom\n");
        return 0;
      }

      printf("[SERVER] Loaded L-system config from file: %s\n", path);
      printf("[SERVER] axiom='%s'\n", cfg->axiom);
      printf("[SERVER] rules=%d iterations=%d (angle=90 fixed)\n", cfg->rule_count, cfg->iterations);
      return 1;
    }
  }

  fclose(fp);
  printf("[SERVER] missing iterations line in file\n");
  return 0;
}

// ---- interactive stdin L-system ----
static void read_line_stdin(const char *prompt, char *out, size_t out_sz) {
  printf("%s", prompt);
  fflush(stdout);
  if (!fgets(out, (int)out_sz, stdin)) {
    out[0] = '\0';
    return;
  }
  rstrip(out);
}

static int read_int_stdin(const char *prompt) {
  char buf[128];
  read_line_stdin(prompt, buf, sizeof(buf));
  return atoi(buf);
}

static int load_lsys_config_interactive(LsysConfig *cfg) {
  memset(cfg, 0, sizeof(*cfg));
  cfg->canvas_w = 80;
  cfg->canvas_h = 40;
  cfg->step = 1;
  cfg->angle_step = 90;
  cfg->start_x = cfg->canvas_w / 2;
  cfg->start_y = cfg->canvas_h / 2;
  cfg->start_a = 90;

  char line[2048];

  read_line_stdin("[SERVER] Podaj aksjomat: ", line, sizeof(line));
  if (line[0] == '\0') {
    printf("[SERVER] ERROR: pusty aksjomat\n");
    return 0;
  }
  strncpy(cfg->axiom, line, sizeof(cfg->axiom) - 1);
  cfg->axiom[sizeof(cfg->axiom) - 1] = '\0';

  cfg->rule_count = read_int_stdin("[SERVER] Podaj liczbe zasad: ");
  if (cfg->rule_count < 0 || cfg->rule_count > MAX_RULES) {
    printf("[SERVER] ERROR: rule_count=%d (max %d)\n", cfg->rule_count, MAX_RULES);
    return 0;
  }

  for (int i = 0; i < cfg->rule_count; i++) {
    char prompt[160];
    snprintf(prompt, sizeof(prompt),
             "[SERVER] Podaj zasade %d/%d (format: X,REPLACEMENT albo X->REPLACEMENT): ",
             i + 1, cfg->rule_count);

    read_line_stdin(prompt, line, sizeof(line));
    if (line[0] == '\0') { i--; continue; }

    Rule r;
    if (!parse_rule_line(line, &r)) {
      printf("[SERVER] ERROR: zly format zasady: '%s'\n", line);
      return 0;
    }
    cfg->rules[i] = r;
  }

  cfg->iterations = read_int_stdin("[SERVER] Podaj liczbe iteracji: ");
  if (cfg->iterations < 0) {
    printf("[SERVER] ERROR: iteracje musza byc >= 0\n");
    return 0;
  }

  printf("[SERVER] Loaded L-system config from stdin\n");
  printf("[SERVER] axiom='%s'\n", cfg->axiom);
  printf("[SERVER] rules=%d iterations=%d \n", cfg->rule_count, cfg->iterations);
  return 1;
}

// infile.txt: start=x:y[:a]
static int load_start_from_infile(const char *path, int *sx, int *sy, int *sa) {
  FILE *fp = fopen(path, "r");
  if (!fp) return 0;

  char line[256];
  while (fgets(line, sizeof(line), fp)) {
    rstrip(line);
    char *p = lstrip(line);
    if (is_ignored_line(p)) continue;

    if (strncmp(p, "start=", 6) == 0) {
      int x = -99999, y = -99999, a = 90;
      int got = sscanf(p + 6, "%d:%d:%d", &x, &y, &a);
      if (got >= 2) {
        *sx = x;
        *sy = y;
        *sa = a;
        fclose(fp);
        return 1;
      }
      printf("[SERVER] ERROR: bad infile format. Use start=x:y or start=x:y:a\n");
      fclose(fp);
      return -1;
    }
  }

  fclose(fp);
  return 0;
}

static int validate_start_or_error(int sx, int sy, int canvas_w, int canvas_h) {
  if (sx < 0 || sx >= canvas_w || sy < 0 || sy >= canvas_h) {
    printf("[SERVER] ERROR: turtle start outside canvas: start=%d:%d canvas=%dx%d\n",
           sx, sy, canvas_w, canvas_h);
    return 0;
  }
  return 1;
}

// -------------------- main --------------------
int main(int argc, char **argv) {
  printf("[SERVER] VERSION: v11-retransmit\n");

  LsysConfig cfg;
  if (argc >= 2) {
    if (!load_lsys_config_file(argv[1], &cfg)) return 1;
  } else {
    if (!load_lsys_config_interactive(&cfg)) return 1;
  }

  int tx = cfg.start_x, ty = cfg.start_y, ta = cfg.start_a;

  int sx = tx, sy = ty, sa = ta;
  int infile_res = load_start_from_infile("infile.txt", &sx, &sy, &sa);
  if (infile_res == -1) return 1;
  if (infile_res == 1) {
    printf("[SERVER] Start loaded from infile.txt: %d:%d:%d\n", sx, sy, sa);
    tx = sx; ty = sy; ta = sa;
  }

  if (!validate_start_or_error(tx, ty, cfg.canvas_w, cfg.canvas_h)) return 1;

  char lsystem[MAX_LSYS_LEN];
  derive_lsystem(cfg.axiom, cfg.rules, cfg.rule_count, cfg.iterations, lsystem, sizeof(lsystem));
  int lsys_len = (int)strlen(lsystem);
  printf("[SERVER] L-system length=%d\n", lsys_len);

  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) { perror("socket"); return 1; }

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(5000);
  server_addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    perror("bind");
    close(sock);
    return 1;
  }

  const int canvas_width  = cfg.canvas_w;
  const int canvas_height = cfg.canvas_h;
  const int total_pixels = canvas_width * canvas_height;

  Node nodes[MAX_NODES];
  int node_count = 0;
  Region regions[MAX_NODES];
  Metrics metrics = {0};

  int started = 0;   // 0 wait nodes, 1 rendering, 2 finishing
  int finishing = 0;

  int current_index = 0;
  int current_node_idx = -1;
  int waiting_ack = 0;

  // retransmission state (no protocol changes)
  const long long RETX_TIMEOUT_MS = 300; // 200..1000 OK
  const int RETX_MAX = 30;              // max retransmits before finish
  long long last_step_send_ms = 0;
  int retx_count = 0;

  // Server stack for [ ]
  int stX[SERVER_STACK_SIZE], stY[SERVER_STACK_SIZE], stA[SERVER_STACK_SIZE];
  int stTop = 0;

  char final_canvas[40][81];
  init_final_canvas(final_canvas, canvas_width, canvas_height);

  printf("[SERVER] WAIT_FOR_NODES (need %d)\n", MAX_NODES);

  while (1) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 200000;

    int ready = select(sock + 1, &readfds, NULL, NULL, &timeout);
    if (ready < 0) { perror("select"); break; }

    // tick
    if (ready == 0) {
      if (!started && node_count >= MAX_NODES) {
        compute_regions(regions, node_count, canvas_width, canvas_height);
        init_final_canvas(final_canvas, canvas_width, canvas_height);

        metrics.handovers = 0;
        metrics.plots_total = 0;
        metrics.plots_unique = 0;

        for (int i = 0; i < node_count; i++) {
          nodes[i].region = regions[i];
          nodes[i].done = 0;

          char msg[MAX_BUF];
          snprintf(msg, sizeof(msg), "ASSIGN_REGION|region=%d:%d:%d:%d:%d",
                   regions[i].idx, regions[i].x_min, regions[i].y_min, regions[i].x_max, regions[i].y_max);
          send_message(sock, &nodes[i].addr, msg, &metrics);

          char init_msg[MAX_BUF];
          snprintf(init_msg, sizeof(init_msg),
                   "INIT_RENDERING|start=%d:%d:%d;step=%d.00;angle=%d.00;canvas=%dx%d",
                   tx, ty, ta, cfg.step, cfg.angle_step, canvas_width, canvas_height);
          send_message(sock, &nodes[i].addr, init_msg, &metrics);
        }

        stTop = 0;

        current_node_idx = 0;
        for (int i = 0; i < node_count; i++) {
          if (region_contains(&nodes[i].region, tx, ty)) { current_node_idx = i; break; }
        }

        char hs[MAX_BUF];
        snprintf(hs, sizeof(hs), "HANDOVER_START|state=%d:%d:%d;index=%d", tx, ty, ta, 0);
        send_message(sock, &nodes[current_node_idx].addr, hs, &metrics);

        started = 1;
        finishing = 0;
        current_index = 0;
        waiting_ack = 0;

        // reset retx
        last_step_send_ms = 0;
        retx_count = 0;

        printf("[SERVER] START_RENDERING with active=%s\n", nodes[current_node_idx].node_id);
      }

      // retransmission if we're waiting for STEP_DONE too long
      if (started == 1 && waiting_ack) {
        long long t = now_ms();
        if (last_step_send_ms != 0 && (t - last_step_send_ms) >= RETX_TIMEOUT_MS) {
          if (retx_count >= RETX_MAX) {
            begin_finish(sock, nodes, node_count, &started, &waiting_ack, &finishing, &metrics,
                         "STEP_DONE timeout (too many retransmissions)");
          } else if (current_index < lsys_len) {
            send_step(sock, nodes, current_node_idx, current_index, lsystem[current_index], &metrics);
            last_step_send_ms = t;
            retx_count++;
            printf("[SERVER] RETX RENDER_STEP idx=%d to %s (try=%d)\n",
                   current_index, nodes[current_node_idx].node_id, retx_count);
          }
        }
      }

      // normal send next step if not waiting
      if (started == 1 && !waiting_ack) {
        if (current_index < lsys_len) {
          send_step(sock, nodes, current_node_idx, current_index, lsystem[current_index], &metrics);
          waiting_ack = 1;
          last_step_send_ms = now_ms();
          retx_count = 0;
        } else {
          begin_finish(sock, nodes, node_count, &started, &waiting_ack, &finishing, &metrics, "lsystem finished");
        }
      }

      continue;
    }

    if (!FD_ISSET(sock, &readfds)) continue;

    char buffer[MAX_BUF];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int len = (int)recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                            (struct sockaddr *)&client_addr, &addr_len);
    if (len <= 0) continue;
    buffer[len] = '\0';
    record_metrics(&metrics, len);

    char *sep = strchr(buffer, '|');
    char msg_type[32];
    char fields[MAX_BUF] = {0};

    if (sep) {
      size_t tlen = (size_t)(sep - buffer);
      if (tlen >= sizeof(msg_type)) tlen = sizeof(msg_type) - 1;
      memcpy(msg_type, buffer, tlen);
      msg_type[tlen] = '\0';
      strncpy(fields, sep + 1, sizeof(fields) - 1);
      fields[sizeof(fields) - 1] = '\0';
    } else {
      strncpy(msg_type, buffer, sizeof(msg_type) - 1);
      msg_type[sizeof(msg_type) - 1] = '\0';
    }

    if (strcmp(msg_type, "REGISTER_NODE") == 0) {
      char node_id[32] = "node";
      (void)parse_field(fields, "id", node_id, sizeof(node_id));

      if (find_node_index(nodes, node_count, node_id) == -1 && node_count < MAX_NODES) {
        Node *node = &nodes[node_count++];
        strncpy(node->node_id, node_id, sizeof(node->node_id) - 1);
        node->node_id[sizeof(node->node_id) - 1] = '\0';

        node->addr = client_addr;
        node->addr.sin_port = htons(NODE_LISTEN_PORT);
        node->done = 0;

        printf("[SERVER] REGISTER_NODE %s (%d/%d) -> %s:%d\n",
               node->node_id, node_count, MAX_NODES,
               inet_ntoa(node->addr.sin_addr), ntohs(node->addr.sin_port));
      }

    } else if (strcmp(msg_type, "PLOT") == 0) {
      char x_raw[16], y_raw[16];
      if (parse_field(fields, "x", x_raw, sizeof(x_raw)) &&
          parse_field(fields, "y", y_raw, sizeof(y_raw))) {

        int x = atoi(x_raw);
        int y = atoi(y_raw);

        if (x >= 0 && x < canvas_width && y >= 0 && y < canvas_height) {
          int row = canvas_height - 1 - y;
          metrics.plots_total++;
          if (final_canvas[row][x] != '#') {
            final_canvas[row][x] = '#';
            metrics.plots_unique++;
          }
        }
      }

    } else if (strcmp(msg_type, "STEP_DONE") == 0) {
      char id[32], idx_raw[16];
      if (parse_field(fields, "id", id, sizeof(id)) &&
          parse_field(fields, "index", idx_raw, sizeof(idx_raw))) {

        if (current_node_idx >= 0 && strcmp(nodes[current_node_idx].node_id, id) == 0) {
          int idx_done = atoi(idx_raw);
          if (idx_done == current_index) {
            char sym = lsystem[current_index];

            // server updates state (keeps in sync with node)
            if (sym == 'F' || sym == 'f') {
              int nx, ny;
              turtle_peek_next(tx, ty, ta, cfg.step, &nx, &ny);
              tx = nx; ty = ny;

            } else if (sym == '+') {
              ta -= cfg.angle_step;

            } else if (sym == '-') {
              ta += cfg.angle_step;

            } else if (sym == '|') {
              ta += 180;

            } else if (sym == '[') {
              if (stTop < SERVER_STACK_SIZE) {
                stX[stTop] = tx; stY[stTop] = ty; stA[stTop] = ta;
                stTop++;
              }

            } else if (sym == ']') {
              if (stTop > 0) {
                stTop--;
                tx = stX[stTop]; ty = stY[stTop]; ta = stA[stTop];
              }
            }

            current_index++;
            waiting_ack = 0;

            // reset retransmission state on success
            retx_count = 0;
            last_step_send_ms = 0;

            if (started == 1 && (tx < 0 || tx >= canvas_width || ty < 0 || ty >= canvas_height)) {
              char reason[200];
              snprintf(reason, sizeof(reason),
                       "turtle moved out of canvas after STEP_DONE (pos=%d,%d idx=%d)",
                       tx, ty, current_index);
              begin_finish(sock, nodes, node_count, &started, &waiting_ack, &finishing, &metrics, reason);
            }
          }
        }
      }

    } else if (strcmp(msg_type, "HANDOVER_REQUEST") == 0) {
      metrics.handovers++;
      if (started != 1) continue;

      char index_raw[16];
      if (!parse_field(fields, "index", index_raw, sizeof(index_raw))) continue;
      int req_index = atoi(index_raw);
      if (req_index != current_index) continue;

      // robust: use next/state from node
      char nextRaw[32], stateRaw[64];
      int nx, ny;
      int sx2 = tx, sy2 = ty, sa2 = ta;

      if (!parse_field(fields, "next", nextRaw, sizeof(nextRaw)) ||
          sscanf(nextRaw, "%d:%d", &nx, &ny) != 2) {
        printf("[SERVER] BAD HANDOVER: missing/invalid next at idx=%d\n", current_index);
        continue;
      }

      if (parse_field(fields, "state", stateRaw, sizeof(stateRaw))) {
        if (sscanf(stateRaw, "%d:%d:%d", &sx2, &sy2, &sa2) != 3) {
          printf("[SERVER] BAD HANDOVER: invalid state at idx=%d\n", current_index);
          continue;
        }
      }

      // if next out of canvas -> finish
      if (nx < 0 || nx >= canvas_width || ny < 0 || ny >= canvas_height) {
        char reason[200];
        snprintf(reason, sizeof(reason), "handover next out of canvas nx,ny=%d,%d idx=%d", nx, ny, current_index);
        begin_finish(sock, nodes, node_count, &started, &waiting_ack, &finishing, &metrics, reason);
        continue;
      }

      int target = -1;
      for (int i = 0; i < node_count; i++) {
        if (region_contains(&nodes[i].region, nx, ny)) { target = i; break; }
      }
      if (target < 0) {
        begin_finish(sock, nodes, node_count, &started, &waiting_ack, &finishing, &metrics,
                     "handover: no node covers next");
        continue;
      }

      if (target == current_node_idx) {
        printf("[SERVER] INFO: HANDOVER_REQUEST but target SAME (%s). next=%d,%d idx=%d\n",
               nodes[target].node_id, nx, ny, current_index);
        continue;
      }

      current_node_idx = target;

      char msg[MAX_BUF];
      snprintf(msg, sizeof(msg), "HANDOVER_START|state=%d:%d:%d;index=%d", sx2, sy2, sa2, current_index);
      send_message(sock, &nodes[current_node_idx].addr, msg, &metrics);

      // allow resend of the SAME step to the new node
      waiting_ack = 0;

      // reset retx (handover is "progress")
      retx_count = 0;
      last_step_send_ms = 0;

      printf("[SERVER] HANDOVER -> %s (state=%d,%d a=%d next=%d,%d) idx=%d\n",
             nodes[current_node_idx].node_id, sx2, sy2, sa2, nx, ny, current_index);

    } else if (strcmp(msg_type, "RENDER_DONE") == 0) {
      char node_id[32];
      if (parse_field(fields, "id", node_id, sizeof(node_id))) {
        int idx = find_node_index(nodes, node_count, node_id);
        if (idx >= 0) {
          nodes[idx].done = 1;
          printf("[SERVER] RENDER_DONE from %s\n", node_id);

          int all_done = 1;
          for (int i = 0; i < node_count; i++) {
            if (!nodes[i].done) { all_done = 0; break; }
          }

          if (all_done) {
            write_output_txt("output.txt", final_canvas, canvas_height);

            double coverage = total_pixels ? 100.0 * (double)metrics.plots_unique / (double)total_pixels : 0.0;

            printf("[SERVER] All nodes done\n");
            printf("[SERVER] Messages: %d\n", metrics.messages);
            printf("[SERVER] Avg packet size: %.2f bytes\n", avg_packet_size(&metrics));
            printf("[SERVER] Handovers: %d\n", metrics.handovers);
            printf("[SERVER] Plots total: %d\n", metrics.plots_total);
            printf("[SERVER] Plots unique: %d / %d (%.2f%%)\n", metrics.plots_unique, total_pixels, coverage);
            break;
          }
        }
      }
    }
  }

  close(sock);
  return 0;
}
