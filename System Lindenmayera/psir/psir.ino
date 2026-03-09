#include <ZsutEthernet.h>
#include <ZsutEthernetUdp.h>
#include <string.h>
#include <stdlib.h>

#define LOGS 1

const ZsutIPAddress serverIp(192, 168, 56, 101);
const unsigned int serverPort = 5000;
const unsigned int localPort  = 5001;

ZsutEthernetUDP Udp;

// ====== CONFIG PER NODE (CHANGE FOR EACH OF 4 NODES) ======
static const char NODE_ID[] = "node-arduino3";
byte macAddress[] = {0x01, 0xff, 0xaa, 0x10, 0x20, 0x35};
const ZsutIPAddress localIp(192, 168, 56, 105);
// ==========================================================

// defaults (overridden by INIT_RENDERING|canvas=WxH)
static int canvasWidth  = 80;
static int canvasHeight = 40;

enum NodeState { IDLE, WORK };
NodeState state = IDLE;

// Region: idx:xmin:ymin:xmax:ymax
int regionParts[5] = {0,0,0,0,0};
bool hasRegion = false;

// Turtle state (INT ONLY)
int turtleX = 40;
int turtleY = 20;
int turtleAngle = 90;    // degrees
int stepLength = 1;      // pixels
int angleStep  = 90;     // degrees

// Stack for []
static const int stackSize = 64;
int stackX[stackSize];
int stackY[stackSize];
int stackAngle[stackSize];
int stackTop = 0;

int currentIndex = 0;
bool handoverSent = false;

// ---- UDP send ----
void sendMessage(const char *msg) {
  Udp.beginPacket(serverIp, serverPort);
  Udp.write((const uint8_t *)msg, strlen(msg));
  Udp.endPacket();
}

void sendRegister() {
  char msg[96];
  snprintf(msg, sizeof(msg), "REGISTER_NODE|id=%s", NODE_ID);
  sendMessage(msg);
#if LOGS
  Serial.print("[NODE] REGISTER sent id=");
  Serial.println(NODE_ID);
#endif
}

// ---- parsing helpers ----
const char *findKey(const char *fields, const char *key) {
  size_t klen = strlen(key);
  const char *p = fields;
  while (p && *p) {
    const char *eq = strchr(p, '=');
    if (!eq) return NULL;
    size_t curr_klen = (size_t)(eq - p);
    if (curr_klen == klen && strncmp(p, key, klen) == 0) return eq + 1;
    const char *semi = strchr(p, ';');
    if (!semi) return NULL;
    p = semi + 1;
  }
  return NULL;
}

bool parseFieldToBuf(const char *fields, const char *key, char *out, size_t outSize) {
  const char *v = findKey(fields, key);
  if (!v) return false;
  const char *end = strchr(v, ';');
  size_t len = end ? (size_t)(end - v) : strlen(v);
  if (len >= outSize) len = outSize - 1;
  memcpy(out, v, len);
  out[len] = '\0';
  return true;
}

bool parseRegion(const char *regionStr) {
  int idx, xMin, yMin, xMax, yMax;
  if (sscanf(regionStr, "%d:%d:%d:%d:%d", &idx, &xMin, &yMin, &xMax, &yMax) == 5) {
    regionParts[0] = idx;
    regionParts[1] = xMin;
    regionParts[2] = yMin;
    regionParts[3] = xMax;
    regionParts[4] = yMax;
    return true;
  }
  return false;
}

inline bool regionContains(int x, int y) {
  if (!hasRegion) return false;
  return x >= regionParts[1] && x <= regionParts[3] && y >= regionParts[2] && y <= regionParts[4];
}

// ---- plot ----
void sendPlot(int x, int y) {
  char msg[64];
  snprintf(msg, sizeof(msg), "PLOT|x=%d;y=%d", x, y);
  sendMessage(msg);
}

// ---- step ack ----
void sendStepDone(int indexDone) {
  char msg[96];
  snprintf(msg, sizeof(msg), "STEP_DONE|id=%s;index=%d", NODE_ID, indexDone);
  sendMessage(msg);
}

// ---- handover (robust: send next + state) ----
void sendHandoverRequestWithNext(int nx, int ny) {
  handoverSent = true;
  char msg[160];
  // state = stan PRZED ruchem
  snprintf(msg, sizeof(msg),
           "HANDOVER_REQUEST|id=%s;index=%d;next=%d:%d;state=%d:%d:%d",
           NODE_ID, currentIndex, nx, ny, turtleX, turtleY, turtleAngle);
  sendMessage(msg);

#if LOGS
  Serial.print("[NODE] HANDOVER_REQUEST idx=");
  Serial.print(currentIndex);
  Serial.print(" next=");
  Serial.print(nx); Serial.print(","); Serial.print(ny);
  Serial.print(" state=");
  Serial.print(turtleX); Serial.print(","); Serial.print(turtleY);
  Serial.print(" a="); Serial.println(turtleAngle);
#endif
}

// ---- grid-int movement (same as server) ----
static void peekNextInt(int x, int y, int angDeg, int step, int *nx, int *ny) {
  int a = angDeg % 360;
  if (a < 0) a += 360;
  *nx = x; *ny = y;

  if (a == 0) (*nx) += step;
  else if (a == 90) (*ny) += step;
  else if (a == 180) (*nx) -= step;
  else if (a == 270) (*ny) -= step;

}

static void normalizeAngle() {
  turtleAngle %= 360;
  if (turtleAngle < 0) turtleAngle += 360;
}

// ---- turtle interpreter ----
void handleSymbol(char symbol) {
  if (symbol == 'F' || symbol == 'f') {
    int nx, ny;
    peekNextInt(turtleX, turtleY, turtleAngle, stepLength, &nx, &ny);

#if LOGS
    Serial.print("[NODE] move ");
    Serial.print(turtleX); Serial.print(","); Serial.print(turtleY);
    Serial.print(" -> ");
    Serial.print(nx); Serial.print(","); Serial.print(ny);
    Serial.print(" ang="); Serial.println(turtleAngle);
#endif

    // handover if NEXT outside this region
    if (hasRegion && !regionContains(nx, ny)) {
      sendHandoverRequestWithNext(nx, ny);
      return;
    }

    // move
    turtleX = nx;
    turtleY = ny;

    // plot only for 'F'
    if (symbol == 'F') {
      if (nx >= 0 && nx < canvasWidth && ny >= 0 && ny < canvasHeight) {
        sendPlot(nx, ny);
      }
    }

  } else if (symbol == '+') {
    turtleAngle -= angleStep;
    normalizeAngle();

  } else if (symbol == '-') {
    turtleAngle += angleStep;
    normalizeAngle();

  } else if (symbol == '|') {
    turtleAngle += 180;
    normalizeAngle();

  } else if (symbol == '[') {
    if (stackTop < stackSize) {
      stackX[stackTop] = turtleX;
      stackY[stackTop] = turtleY;
      stackAngle[stackTop] = turtleAngle;
      stackTop++;
    }

  } else if (symbol == ']') {
    if (stackTop > 0) {
      stackTop--;
      turtleX = stackX[stackTop];
      turtleY = stackY[stackTop];
      turtleAngle = stackAngle[stackTop];
    }
  }
}

void setup() {
#if LOGS
  Serial.begin(9600);
  delay(200);
  Serial.println("[NODE] Boot");
#endif

  ZsutEthernet.begin(macAddress, localIp);
  Udp.begin(localPort);
  delay(500);

  sendRegister();
}

void loop() {
  int packetSize = Udp.parsePacket();
  if (!packetSize) return;

  char buf[256];
  int len = Udp.read(buf, sizeof(buf) - 1);
  if (len <= 0) return;
  buf[len] = '\0';

  char *sep = strchr(buf, '|');
  char *type = buf;
  char *fields = (char *)"";
  if (sep) { *sep = '\0'; fields = sep + 1; }

  if (strcmp(type, "ASSIGN_REGION") == 0) {
    char regionStr[64];
    if (parseFieldToBuf(fields, "region", regionStr, sizeof(regionStr))) {
      if (parseRegion(regionStr)) {
        hasRegion = true;
#if LOGS
        Serial.print("[NODE] ASSIGN_REGION ");
        Serial.println(regionStr);
#endif
      }
    }

  } else if (strcmp(type, "INIT_RENDERING") == 0) {
    char startStr[64], stepStr[16], angleStr[16], canvasStr[32];

    if (parseFieldToBuf(fields, "start", startStr, sizeof(startStr))) {
      int x, y, a;
      if (sscanf(startStr, "%d:%d:%d", &x, &y, &a) == 3) {
        turtleX = x; turtleY = y; turtleAngle = a;
        normalizeAngle();
      }
    }

    if (parseFieldToBuf(fields, "step", stepStr, sizeof(stepStr))) {
      stepLength = (int)atof(stepStr);
      if (stepLength <= 0) stepLength = 1;
    }

    if (parseFieldToBuf(fields, "angle", angleStr, sizeof(angleStr))) {
      angleStep = (int)atof(angleStr);
      if (angleStep == 0) angleStep = 90;
    }

    if (parseFieldToBuf(fields, "canvas", canvasStr, sizeof(canvasStr))) {
      int w, h;
      if (sscanf(canvasStr, "%dx%d", &w, &h) == 2) { canvasWidth = w; canvasHeight = h; }
    }

    stackTop = 0;
    currentIndex = 0;
    state = IDLE;

#if LOGS
    Serial.println("[NODE] INIT_RENDERING -> IDLE (wait HANDOVER_START)");
#endif

  } else if (strcmp(type, "HANDOVER_START") == 0) {
    char stateStr[64], indexStr[16];

    if (parseFieldToBuf(fields, "state", stateStr, sizeof(stateStr))) {
      int x, y, a;
      if (sscanf(stateStr, "%d:%d:%d", &x, &y, &a) == 3) {
        turtleX = x; turtleY = y; turtleAngle = a;
        normalizeAngle();
      }
    }
    if (parseFieldToBuf(fields, "index", indexStr, sizeof(indexStr))) {
      currentIndex = atoi(indexStr);
    }

    state = WORK;

#if LOGS
    Serial.print("[NODE] HANDOVER_START -> WORK idx=");
    Serial.println(currentIndex);
#endif

  } else if (strcmp(type, "RENDER_STEP") == 0 && state == WORK) {
    char indexStr[16], symbolStr[8];

    if (parseFieldToBuf(fields, "index", indexStr, sizeof(indexStr))) {
      currentIndex = atoi(indexStr);
    }

    if (parseFieldToBuf(fields, "symbol", symbolStr, sizeof(symbolStr))) {
#if LOGS
      Serial.print("[NODE] STEP idx=");
      Serial.print(currentIndex);
      Serial.print(" sym=");
      Serial.println(symbolStr[0]);
#endif

      handoverSent = false;
      int executedIndex = currentIndex;

      handleSymbol(symbolStr[0]);

      if (!handoverSent) {
        sendStepDone(executedIndex);
      }
    }

  } else if (strcmp(type, "RENDER_DONE") == 0) {
    char idStr[32];
    if (parseFieldToBuf(fields, "id", idStr, sizeof(idStr)) && strcmp(idStr, "server") == 0) {
      char msg[96];
      snprintf(msg, sizeof(msg), "RENDER_DONE|id=%s", NODE_ID);
      sendMessage(msg);
      state = IDLE;

#if LOGS
      Serial.println("[NODE] RENDER_DONE -> IDLE");
#endif
    }
  }
}
