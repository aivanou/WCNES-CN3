import sys
import networkx as nx
import matplotlib.pyplot as plt


def drawGraph(G, title, n):
	# Find the sub-optimal edges
	pos = nx.spring_layout(G) # positions for all nodes
	plt.subplot(220 + n)
	plt.title(title)
	plt.axis('off')
	nx.draw_networkx_labels(G, pos, font_size = 12, font_family = 'sans-serif')
	# nodes
	nx.draw_networkx_nodes(G, pos, cmap = plt.get_cmap('jet'), node_size=750)
	# edges
	nx.draw_networkx_edges(G, pos, edge_color = 'black', arrows = True)
	# edge labels
	edge_labels=dict([((u,v,),"%.2f" % float(d['weight'])) for u,v,d in G.edges(data=True)])

	nx.draw_networkx_edge_labels(G, pos, edge_labels=edge_labels, label_pos=0.75, font_color='black')
	


def parse_file(filename):
	get_node_id=lambda l: int(l.split('.')[0])*256+int(l.split('.')[1])
	output_lst=list()
	for line in open(filename,'r').readlines():
		data = line.strip().split(' ')
		node_from_id = get_node_id(data[0]) 
		node_to_id = get_node_id(data[1])
		total_packets=int(data[2])
		lost_packets=int(data[3])
		lqi=int(data[4])
		rssi=int(data[5])
		etx=int(data[6])
		etx_acc=int(data[7])
		output_lst.append({'node_from':node_from_id,'node_to':node_to_id,
						   'total_packets':total_packets,'lost_packets':lost_packets,
						   'lqi':lqi,'rssi':rssi,'etx':etx,'etx_acc':etx_acc})
	return output_lst


"""
dejkstra algorithm for finding optimal sink using HOP distance
TODO: implement ETX metric and find optimal sink using ETX metric
"""

INF=10000


def dejkstra(g,startNode,metric):
	for nId in g.nodes():
		g.node[nId]['visited']=False
		g.node[nId]['metric']=INF
		g.node[nId]['parent']=None
	g.node[startNode]['metric']=0
	a = attendNodes(g,startNode,metric)
	return a

def attendNodes(g,current_node_id,metric):
	node=g.node[current_node_id]
	node['visited']=True
	nbrs=g.neighbors(current_node_id)
	for n_id in nbrs:
		n=g.node[n_id]
		if not 'metric' in n or n['metric']==None or n['metric'] > metric(g,node,n):
			n['metric']=metric(g,node,n)
			n['parent']=current_node_id
	new_node_id=None
	for nId in nbrs:
		n=g.node[nId]
		if not 'visited' in n or not n['visited']:
			new_node_id=nId
			break
	if new_node_id == None: 
		print 'returning ',g.nodes()
		return g
	return attendNodes(g,new_node_id,metric)


def hop_metric(g,node,child):
	if not 'metric' in node:
		node['metric']=0
	return node['metric'] + 1

def calculate_sum_metric(g):
	return sum([g.node[n]['metric'] for n in g.nodes()])

def find_optimal_sink_by_hop(g):
	min_weihgt=None
	sink=None
	return min([(calculate_sum_metric(dejkstra(g,x,hop_metric)),x) for x in g.nodes()])


def gen_graph(lst):
	g=nx.DiGraph()
	for rec in lst:
		g.add_edge(rec['node_from'],rec['node_to'])
	return g

g=nx.DiGraph()
g.add_edges_from([(1,2),(1,3),(1,6),(1,7),(2,4),(4,5),(5,6),(6,7),(6,8),(7,9),(8,9),(9,8)])
# print len(g[1])
# print g.nodes()
# g=dejkstra(g,1,hop_metric)

# for nId in g.nodes():
# 	n=g.node[nId]
# 	print nId,n['parent'],n['metric']

# print sum([g.node[n]['metric'] for n in g.nodes()])
# print min(None,10)

# find_optimal_sink_by_hop(g)

# for n,nbrs in FG.adjacency_iter():
# 	print nbrs
# 	for nbr,eattr in nbrs.items():
# 		data=eattr['weight']
# 		print('(%d, %d, %.3f)' % (n,nbr,data))

