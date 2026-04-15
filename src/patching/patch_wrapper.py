#!/usr/bin/env python3
import sys
import os
import tempfile
import shutil
import csv
import subprocess

def args_match(a,b):
    for i in range(len(a)):
        if a[i] != b[i]: return False
    return True

def parse_patch_points(csv_in, debug=False, pifr_id=0):
    pifr_map = {}
    patch_points = {
            # maps addr -> (id,args), helps search and uniq
            'write':{},
            'indirect_write':{},
            'read':{},
            'indirect_read':{},
            # simple lists
            'flush': [],
            'release': []
            }
    with open(csv_in, 'r') as f:
        for l in f.readlines():
            s = l.strip().split(',')
            key = s[0]
            rest = s[1:]
            if key in ('write',  'read', 'indirect_write', 'indirect_read'):
                start_addr = rest[0]
                acc_addr = rest[1]
                args = tuple(rest[2:])
                # start_addr, acc_addr, base_mask, scale, index_mask, displacement, size = rest
                # args = (start_addr, base_mask, scale, index_mask, displacement, size)
                if start_addr in patch_points[key]:
                    neighbours = patch_points[key][start_addr]
                    for n in neighbours:
                        n_id, n_args = n
                        if args_match(n_args, args):
                            pifr_map[n_id].append(acc_addr)
                            break
                    else: # if there is no match, create new id but only add to addr
                        pifr_map[pifr_id] = [acc_addr]
                        patch_points[key][start_addr] += [(pifr_id,args)]
                        pifr_id += 1
                else: # create new id and create new addr
                    pifr_map[pifr_id] = [acc_addr]
                    patch_points[key][start_addr] = [(pifr_id,args)]
                    pifr_id += 1
            elif key in ('flush', 'release'):
                if rest not in patch_points[key]:
                    patch_points[key] += (rest,)
            else:
                print("ERROR: this shouldn't happen")
    return (patch_points, pifr_map)

def patch(target, target_out, patch_points, debug=False, lib=False):
    e9tool = os.path.join(os.environ['E9ROOT'], 'e9tool')
    patcher = os.path.join(os.environ['MUMAK_ROOT'], 'src/patching/patcher')
    proc = subprocess.Popen(f'objdump -d -M intel {target} | grep "<_start>:" | cut -d\' \' -f1', shell=True, stdout=subprocess.PIPE)
    init = proc.stdout.read().decode().strip()

    with tempfile.TemporaryDirectory() as dir:
        shutil.copy2(patcher, dir)
        patch_opts = ''
        if not lib: patch_opts += f' -M offset=0x{init} -P "patch_set_offset(addr,(static)addr)@patcher"'
        for key in ('write',  'read', 'indirect_write', 'indirect_read'):
            points = patch_points[key]
            counter = 0
            while len(points):
                label = f'{key}_{counter}'
                counter += 1
                rows = []
                for addr in list(points.keys()):
                    rows.append((addr, points[addr][0][0],) + points[addr][0][1])
                    if len(points[addr]) == 1:
                        del points[addr]
                    else:
                        points[addr] = points[addr][1:]
                with open(os.path.join(dir, f'{label}.csv'), 'w') as f:
                    writer = csv.writer(f)
                    writer.writerows(rows)
                    if 'indirect' in key:
                        patch_opts += f' -M offset={label}[0] -P "after patch_start_pifr_{key}({label}[1],{label}[2],{label}[3],{label}[4],{label}[5],{label}[6],{label}[7],state)@patcher"'
                    else:
                        patch_opts += f' -M offset={label}[0] -P "after patch_start_pifr_{key}({label}[1],{label}[2],{label}[3],{label}[4],{label}[5],{label}[6],state)@patcher"'

            with open(os.path.join(dir, f'flush.csv'), 'w') as f:
                writer = csv.writer(f)
                writer.writerows(patch_points['flush'])
                patch_opts += f' -M offset=flush[0] -P \"patch_flush(flush[0],flush[1],flush[2],flush[3],flush[4],flush[5],state)@patcher\"'

            with open(os.path.join(dir, f'release.csv'), 'w') as f:
                writer = csv.writer(f)
                writer.writerows(patch_points['release'])
                patch_opts += f' -M offset=release[0] -P "patch_end_pifrs(release[0])@patcher"'

        cmd = f'{e9tool} --syntax intel --option -Ocall {"--option --debug=true" if debug else ""} {patch_opts} {target} -o {target_out}'
        if debug: print(cmd)

        res = subprocess.call(cmd, shell=True, cwd=dir)
        if res and not debug:
            print(f'Input: {cmd}')
        return res

def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('target', help='path to target binary')
    parser.add_argument('patch_points', help='path to palantir\'s output patch point csv')
    parser.add_argument('--lib', help="target is a library", action="store_true")
    parser.add_argument('--debug', help="increase output verbosity", action="store_true")
    parser.add_argument('--pifr_id', help="start id for pifrs")
    args = parser.parse_args()

    cwd = os.getcwd()
    target = os.path.realpath(args.target)
    target_basename = os.path.splitext(os.path.basename(target))[0].replace("-", "_")
    target_out = os.path.join(cwd, target_basename + '.patched')

    pps, pifr_map = parse_patch_points(args.patch_points, debug=args.debug, pifr_id=int(args.pifr_id)+1 if args.pifr_id else 0)
    res = patch(target, target_out, pps, debug=args.debug, lib=args.lib)

    if res: sys.exit(res)

    import json
    with open(target_basename + '_pifrs.json', 'w') as f:
        json.dump(pifr_map, f)

if __name__ == '__main__':
    main()
