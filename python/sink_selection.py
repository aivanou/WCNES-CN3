import sys
import networkx as nx
import matplotlib.pyplot as plt
import re


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
	


def parseFile(fileName):
	get_node_id=lambda l: int(l.split('.')[0])*256+int(l.split('.')[1])
	print "parsing file ", fileName
	f = open(fileName, 'r')
	nodes={}
	for line in f:
		data = re.findall("[A-Za-z:,\ ]*([\-]*[0-9\.]+)", line)
		if len(data) != 9:
			continue
		data = [d for d in data if len(d)>0]
		u = data[0].strip()
		v = data[1].strip()
		if not get_node_id(u) in nodes:
			nodes[get_node_id(u)] = list()
		if not get_node_id(v) in nodes[get_node_id(u)]:
			nodes[get_node_id(u)].append(get_node_id(v))	
	
	for u in nodes.keys():
		for v in nodes[u]:
			if not v in nodes.keys():
				nodes[v]=list()
			if not u in nodes[v]:
				nodes[v].append(u)
	return nodes


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
	a = attendNodes(g,startNode)
	return a

def attendNodes(g,nodeId):
	stack_nodes = list()
	stack_nodes.append(nodeId)
	while len(stack_nodes) > 0:
		nodeId = stack_nodes[0]
		g.node[nodeId]['visited']=True
		for nId in g.neighbors(nodeId):
			if not 'metric' in g.node[nId] \
			   or g.node[nId]['metric'] == -1 or g.node[nId]['metric'] > g.node[nodeId]['metric'] + 1:
				g.node[nId]['metric']=g.node[nodeId]['metric']+1
				g.node[nId]['parent']=nodeId

		for nId in g.neighbors(nodeId):
			if not 'visited' in g.node[nId] or not g.node[nId]['visited']:
				stack_nodes.append(nId)
		stack_nodes.remove(nodeId)
	return g

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

# g=nx.DiGraph()
# g.add_edges_from([(1,2),(1,3),(1,6),(1,7),(2,4),(4,5),(5,6),(6,7),(6,8),(7,9),(8,9),(9,8)])
# # print len(g[1])
# print g.nodes()
# g=dejkstra(g,1,hop_metric)

# print sum([g.node[n]['metric'] for n in g.nodes()])

# print find_optimal_sink_by_hop(g)


if __name__ == "__main__":

	fname = "graph.txt"

	g=nx.DiGraph()

	nodes= parseFile(fname)
	
	for u,vs in nodes.items():
		for v in vs:
			g.add_edge(u,v)

	for n in g.nodes():
		g.node[n]['color']=0

	optimal_sink = find_optimal_sink_by_hop(g)
	print optimal_sink
