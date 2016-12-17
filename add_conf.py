import urllib, urllib2
import json

#config_dict = {"type": "PING", "times":4, "frenc": 12, "interval": 500, "host": "10.88.15.94", "timeout": 2000, "ttl": 64, "id":3}
#config_dict = {"type": "PING", "times":4, "frenc": 12, "interval": 500, "host": "10.88.15.94", "timeout": 2000, "ttl": 64, "id":3}
config_dict = {"type": "PING", "times":4, "frenc": 8, "interval": 500, "host": "10.88.15.63", "timeout": 2000, "ttl": 64, "id":31}
#config_dict = {"type": "PING", "times":3, "frenc": 20, "interval": 500, "host": "180.149.139.43", "timeout": 2000, "ttl": 64, "id":30}



urlstr = "http://127.0.0.1:11111/metric"


config_json = json.dumps(config_dict)
print config_json
req = urllib2.Request(url=urlstr, data=config_json)
req.add_header('Content-Type', "application/json")
req.add_header('Content-Length', str(len(config_json)))
f = urllib2.urlopen(req)
f.read()


