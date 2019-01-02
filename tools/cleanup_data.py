import sys

filepath = sys.argv[1]
function = sys.argv[2]

function_name = "/function/%s" % function
system_call = "/system/function/%s" % function

with open(filepath) as fp:
   line = fp.readline()
   cnt = 1
   while line:
       if function_name not in line:
           line = fp.readline()
           continue
       elif system_call in line:
           line = fp.readline()
           continue
       else:
           print(line.strip().split()[9])
       line = fp.readline()
       cnt += 1
