
## show tuple info in relation file

```
osdba-mac:pg_fix osdba$ ./pg_fix show_tuple -f 16384
 lp   off  flags lp_len    xmin       xmax      field3   blkid posid infomask infomask2 hoff     oid
---- ----- ----- ------ ---------- ---------- ---------- ----- ----- -------- --------- ----  ----------
   1  8152     1     33       1001       1002          0     0     3        2      4002   24          0
   2  8112     1     33       1001       1002          0     0     4        2      4002   24          0
   3  8072     1     33       1002          0          0     0     3     2002      8002   24          0
   4  8032     1     33       1002          0          0     0     4     2002      8002   24          0
```

## Clean hint in tuple

first show tuple in replation file 16384:

```
osdba-mac:pg_fix osdba$ ./pg_fix show_tuple -f 16384
 lp   off  flags lp_len    xmin       xmax      field3   blkid posid infomask infomask2 hoff     oid
---- ----- ----- ------ ---------- ---------- ---------- ----- ----- -------- --------- ----  ----------
   1  8152     1     33       1001       1002          0     0     3      502      4002   24          0
   2  8112     1     33       1001       1002          0     0     4      502      4002   24          0
   3  8072     1     33       1002          0          0     0     3     2902      8002   24          0
   4  8032     1     33       1002          0          0     0     4     2902      8002   24          0
```

then clean hint then show tuple info, please notice "infomask" colume:

```
osdba-mac:pg_fix osdba$ ./pg_fix clean_tuple_hint -f 16384
osdba-mac:pg_fix osdba$ ./pg_fix show_tuple -f 16384
 lp   off  flags lp_len    xmin       xmax      field3   blkid posid infomask infomask2 hoff     oid
---- ----- ----- ------ ---------- ---------- ---------- ----- ----- -------- --------- ----  ----------
   1  8152     1     33       1001       1002          0     0     3        2      4002   24          0
   2  8112     1     33       1001       1002          0     0     4        2      4002   24          0
   3  8072     1     33       1002          0          0     0     3     2002      8002   24          0
   4  8032     1     33       1002          0          0     0     4     2002      8002   24          0
```

## Query transaction status in commit log and change


query status of transaction:

```
osdba-mac:pg_fix osdba$ ./pg_fix get_xid_status -f 0000.bkk -x 11
xid(11) status is 1(COMMITTED)
```

set status of transaction:

```
osdba-mac:pg_fix osdba$ ./pg_fix set_xid_status -f 0000.bkk -x 11 -s 0
xid(11) status from 1(COMMITTED) change to 0(IN_PROGRESS)
```
