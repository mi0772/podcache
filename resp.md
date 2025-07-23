# Algoritmo RESP - Guida Implementation

## Tipi di dato RESP:

1. **Simple String**: `+OK\r\n`
2. **Error**: `-ERR message\r\n`
3. **Integer**: `:1000\r\n`
4. **Bulk String**: `$6\r\nfoobar\r\n`
5. **Array**: `*2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n`

## Algoritmo di PARSING (dal client):

### 1. **Leggi il primo byte**
- `*` → Array
- `$` → Bulk String
- `+` → Simple String
- `-` → Error
- `:` → Integer

### 2. **Se è Array (`*`)**:
```
*3\r\n$3\r\nSET\r\n$5\r\nmykey\r\n$5\r\nvalue\r\n
```

- Leggi numero fino a `\r\n` → `3` elementi
- **Loop 3 volte:**
    - Per ogni elemento, richiama ricorsivamente il parser
    - Elemento 1: `$3\r\nSET\r\n` → string "SET"
    - Elemento 2: `$5\r\nmykey\r\n` → string "mykey"
    - Elemento 3: `$5\r\nvalue\r\n` → string "value"

### 3. **Se è Bulk String (`$`)**:
```
$11\r\nhello world\r\n
```

- Leggi numero fino a `\r\n` → `11` (lunghezza)
- Leggi esattamente 11 byte → `"hello world"`
- Leggi e salta `\r\n`

**Caso speciale**: `$-1\r\n` = NULL

### 4. **Se è Simple String (`+`)**:
```
+OK\r\n
```

- Leggi tutto fino a `\r\n` → `"OK"`

## Algoritmo di RISPOSTA (dal server):

### Per OK semplice:
```c
send(socket, "+OK\r\n", 5, 0);
```

### Per string con contenuto:
```c
char *value = "hello world";
char response[256];
sprintf(response, "$%zu\r\n%s\r\n", strlen(value), value);
send(socket, response, strlen(response), 0);
```

### Per NULL:
```c
send(socket, "$-1\r\n", 5, 0);
```

### Per errore:
```c
send(socket, "-ERR unknown command\r\n", 21, 0);
```

## Stati del parser (macchina a stati):

1. **READ_TYPE** → leggi `*`, `$`, `+`, `-`, `:`
2. **READ_LENGTH** → leggi numero fino `\r\n`
3. **READ_DATA** → leggi esattamente N byte
4. **READ_CRLF** → consuma `\r\n`
5. **DONE** → elemento completato

## Punti critici:

- **Non usare `\n` come delimitatore** → sempre `\r\n`
- **Bulk string può contenere `\r\n`** → usa la lunghezza prefissata
- **Array può essere nested** → ricorsione
- **Gestisci buffer parziali** → comando può arrivare spezzettato su più recv()

## Strategia implementation:

1. **Buffer circolare** per gestire dati parziali
2. **State machine** per parsing incrementale
3. **Stack o ricorsione** per array nested

Il trucco è gestire che un comando può arrivare in più chiamate `recv()` separate!