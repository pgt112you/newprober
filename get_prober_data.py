import redis
import time


robj = redis.Redis(host="127.0.0.1", port=6379)
while (1):
    res = robj.rpop("proberlist")
    if not res:
        time.sleep(1)
        continue
    res = res.strip("\n")
    print res
