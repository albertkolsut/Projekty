import bcrypt

dane_pracownikow = {
    'jakub.surma': 'Password123',
    'albert.kolsut': 'latwe123',
    'kajetan.wilkowski': 'jazda',
    'ofiara.phishing': 'zaq1@WSX',
    'marek.testowy': 'razdwatrzy'
}

print("--- START GENEROWANIA HASHY BCrypt ---")
instrukcje_sql = []

for login, haslo_czysty_tekst in dane_pracownikow.items():
    haslo_bytes = haslo_czysty_tekst.encode('utf-8')
    sol = bcrypt.gensalt(rounds=12)
    hash_bytes = bcrypt.hashpw(haslo_bytes, sol)
    hash_string = hash_bytes.decode('utf-8')

    sql_command = f"('{login}', '{hash_string}')"
    instrukcje_sql.append(sql_command)

    print(f"\nLogin: {login}")
    print(f"Hasło: {haslo_czysty_tekst}")
    print(f"Hash:  {hash_string}")

print("\n--- ZAHASHOWANE DANE ---")



final_insert_sql = (
        ",\n".join(instrukcje_sql) + ";"
)

print(final_insert_sql)
