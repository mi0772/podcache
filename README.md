# PodCache

Server cache locale per pod, redis compatibile

## TODO's list

1) implementare la cache LRU come insieme di lru_cache, dimensionata in base
   alla capacità richiesta, come per le partizioni kafka, ogni chiave verrà
   indirizzata alla partizione corretta secondo la logica HASH % NUM_PARTITION
2) Per la cache su disco, va previsto un meccanismo che pulisca il filesystem alla destroy della cache,
   per farlo è necessario che alla create, tutti i file e le directory vanno messe sotto una dir
   univoca generata durante la create della pod_cache
3) è ridondante creare la struttura con l'ultima parte dell'hash come dir, e poi inserire il file value.dat all'interno
   usare l'ultima chunk dell'hash come file 