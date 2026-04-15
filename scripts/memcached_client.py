import threading
from numpy import random
from pymemcache.client.base import Client, MemcacheClientError

def get_random_dist_integer(key_range: int, n: int, zipf: bool =False):
    dist = []
    if zipf:
        random.normal(loc=key_range/2, scale=key_range/40, size=n)
    else:
        dist = random.zipf(a=2, size=n)
    return list(map(int, dist))

cmds = [
        lambda conn, key:  conn.set(key,key),
        lambda conn, key:  conn.add(key,key),
        lambda conn, key:  conn.replace(key,key),
        lambda conn, key:  conn.append(key,key),
        lambda conn, key:  conn.prepend(key,key),
        lambda conn, key:  conn.cas(key,key,key),
        lambda conn, key:  conn.delete(key),
        lambda conn, key:  conn.incr(key,1),
        lambda conn, key:  conn.decr(key,1),
        lambda conn, key:  conn.get(key),
        # lambda conn, key:  conn.flush_all(),
        ]

def client_loop(id: int, connection: Client, n_ops: int, key_range: int, zipf: bool=False):
    len_cmds = len(cmds)
    segment_size = n_ops // 5
    for i, k in enumerate(get_random_dist_integer(key_range, n_ops, zipf)):
        cmd = cmds[(i+k)%len_cmds]
        try: 
            cmd(connection, str(k))
        except MemcacheClientError as e:
            print(f"[{id}] {e}")
        if i % segment_size == 0:
            print(f"[{id}] {i*100//n_ops}%")
    connection.close()

def load(connection: Client, start: int, n: int):
    for x in range(n):
        connection.set(str(start+x), str(start+x))
    connection.close()

def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('endpoint', help='memcached endpoint')
    parser.add_argument('threads', help='number of concurrent threads')
    parser.add_argument('workload_size', help='workload size')
    parser.add_argument('--zipf', help='use zipf distribution', action="store_true")
    parser.add_argument('--seed', help='random generator seed')
    args = parser.parse_args()

    random.seed(args.seed if args.seed else 1)

    n_threads = int(args.threads)
    workload_size = int(args.workload_size)
    key_range = workload_size
    endpoint = args.endpoint
    c = Client(args.endpoint, default_noreply=True)

    print("Loading...")
    segment = workload_size // n_threads
    threads = []
    for x in range(n_threads):
        thread = threading.Thread(target=load, args=(Client(endpoint, default_noreply=False), segment*x, segment))
    for thread in threads:
        thread.join()
    print("Load phase ended.")

    threads.clear()
    for x in range(n_threads):
        thread = threading.Thread(target=client_loop, args=(x, Client(endpoint, default_noreply=False), workload_size//n_threads, key_range, args.zipf))
        thread.start()
        threads.append(thread)
    
    for thread in threads:
        thread.join()
    
    c.shutdown()
    print("Done.")

if __name__ == '__main__':
    main()
        






