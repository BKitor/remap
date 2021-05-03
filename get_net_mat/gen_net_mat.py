#!/home/bkitor/Projects/remap/get_net_mat/.venv/bin/python
import sys
import networkx
import argparse
import re
import pprint
pp = pprint.PrettyPrinter()


def gen_graph(slurm_topo_file: str) -> networkx.Graph:
    rc1 = re.compile(r'^SwitchName=(.*) (Switches|Nodes)=(.*)$')
    g = networkx.Graph()
    with open(slurm_topo_file, "r") as f:
        for line in f:
            if not line.startswith("SwitchName"):
                continue

            m = rc1.match(line)
            g.add_node(m.group(1))
            if m.group(2) == "Nodes":
                # can be either:
                # cdr[876-904,908,910-911]
                # blg[8510-8521],blg[8610-8625],blg8699
                # blg8510,blg[8610-8625],blg8699
                rc2 = re.compile(r'^[a-z]{3}(\[[-\d]+\]|\d+),')
                connected_nodes = []
                glb_str = m.group(3)
                while rc2.match(glb_str):
                    m2 = rc2.match(glb_str)
                    matched_substr = m2.group(0)[:-1]
                    connected_nodes.extend(expand_node_list(matched_substr))
                    glb_str = glb_str.replace(m2.group(0), '')

                connected_nodes.extend(expand_node_list(glb_str))

            elif m.group(2) == "Switches":
                connected_nodes = m.group(3).split(',')

            else:
                raise Exception(
                    f"ERROR: rc1.group(2) was neither Nodes or Switches: {rc1.group(2)}")

            for n in connected_nodes:
                g.add_node(n)
                g.add_edge(m.group(1), n)
    return g


# takes a string of slurm's nodes and formats it into a list of nodes
# "cdr[101-103, 201]" -> ['cdr101', 'cdr102', 'cdr103', 'cdr201']
def expand_node_list(in_str: str) -> list:
    ret_arr = []
    rp = re.compile(r'^([a-z]{3})\[(.*)\]$')
    m = rp.match(in_str)

    if m is None:
        rp2 = re.compile(r'^[a-z]{3}\d+$')
        if rp2.match(in_str):
            return [in_str]
        else:
            raise Exception(f"Bad String {in_str}")

    for n in m.group(2).split(','):
        if '-' in n:
            s = n.split('-')
            for i in range(int(s[0]), int(s[1])+1):
                ret_arr.append(m.group(1)+str(i))
        else:
            ret_arr.append(m.group(1)+n)

    return ret_arr


def gen_hname_lid_dicts(ib_file: str) -> (dict, dict):
    # get hostname and lid form sample inptus
    # Ca	1 "H-506b4b0300c356b8"		# "blg8429 mlx5_0"
    # [1](506b4b0300c356b8) 	"S-506b4b03005d3100"[28]		# lid 786 lmc 0 "SwitchIB Mellanox Technologies" lid 2 4xEDR
    rc_hname = re.compile(r'^Ca.*([a-z]{3}\d+) mlx5_0')
    rc_lid = re.compile(r'^.* lid (\d+) .* lid (\d+) .*$')
    h2l_dict = {}
    l2h_dict = {}
    with open(ib_file, "r") as f:
        f_l = list(f)
        for i, line in enumerate(f_l):
            m_hname = rc_hname.match(line)
            if not m_hname:
                continue
            m_lid = rc_lid.match(f_l[i+1])
            if not m_lid:
                print("BIG OPPSIE with", m_hname.group(1))
                continue
            # print(m_hname.group(1), m_lid.group(1), m_lid.group(2))
            l2h_dict[m_lid.group(1)] = m_hname.group(1)
            h2l_dict[m_hname.group(1)] = m_lid.group(1)

    return l2h_dict, h2l_dict


def gen_remap_matrix(g: networkx.Graph, l2h_dict: dict, h2l_dict: dict):
    dist_dict = {}
    max_dist_arr = []
    for h_name, tmp_d in networkx.all_pairs_shortest_path_length(g):
        if h_name not in l2h_dict.values():
            continue
        dist_dict[h_name] = tmp_d
        max_dist_arr.append(max(tmp_d.values(), key=lambda x: int(x)))

    max_lid = max(l2h_dict, key=lambda x: int(x))
    max_lid_num = int(max_lid)
    max_hb = int(max(max_dist_arr, key=lambda x: int(x)))

    def z_line(lid):
        return (
            "0 " * (lid) + "{0:01} ".format(max_hb) + "0 " * (max_lid_num - lid - 1))

    # print(len(dist_dict))
    print(max_lid_num)
    for i in range(0, int(max_lid)):
    # for i in range(0, 9):
        if str(i) not in l2h_dict or l2h_dict[str(i)] not in dist_dict:
            # print(f"{i} is not in")
            print(z_line(i))
            continue

        cur_hname = l2h_dict[str(i)]
        cur_ddict = dist_dict[cur_hname]
        outstr = ""
        for i in range(max_lid_num):
            if str(i) in l2h_dict and l2h_dict[str(i)] in cur_ddict:
                outstr += str(max_hb - cur_ddict[l2h_dict[str(i)]]) + " "
            else:
                outstr += "0 "
        print(outstr)

        # print("d is ", max_hb - cur_ddict[l2h_dict[str(666)]])

        # print(f"{i} is {l2h_dict[str(i)]}")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '-s', type=str, help="slurm topology file", required=False)
    parser.add_argument(
        '-i', type=str, help="ibnetdiscover file", required=False)
    parser.add_argument('-g', help="output graphviz", action='store_true')
    parser.add_argument('-r', help="output remap_matrix", action='store_true')
    args = parser.parse_args()

    if args.s:
        g = gen_graph(args.s)

    if args.i:
        l2h_dict, h2l_dict = gen_hname_lid_dicts(args.i)

    if args.g:
        networkx.drawing.nx_pydot.write_dot(g, sys.stdout)

    if args.r:
        if args.s and args.i:
            gen_remap_matrix(g, l2h_dict, h2l_dict)
        else:
            print("Need vals for -s and -i if you wanna use -r", file=sys.stderr)

    # print(len(g.nodes), len(g.edges))
    # expand_node_list('cdr[340-349,385-386,517,520,523,912,991-994]')
    # pp.pprint(g.edges)
    # print(len(g.nodes), len(set(g.nodes)))
    # print(g.degree())
    # print(networkx.algorithms.distance_measures.eccentricity(g))
    # print(g.nodes)
    # print(l2h_dict.values())
    # print(next(networkx.all_pairs_shortest_path_length(g)))
