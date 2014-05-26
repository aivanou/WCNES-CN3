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

"""
Centralized CDS for wireless network
"""

"""
computes weight of 2 nodes
ex , ey - energy measurments( not used now )
Nx,Ny  - degree of the nodes
IDx,IDy - node ids
"""
def weight(ex,ey,Nx,Ny,IDx,IDy):
	def compare(lst):
		if len(lst)==0:
			return 0
		if(cmp(lst[0],lst[1])==0):
			return compare(lst[2:])
		return cmp(lst[0],lst[1])
	return compare([ex,ey,Nx,Ny,IDx,IDy])

def max_weight_node(g,nodes_id,allowed_colors):
	if len(nodes_id) == 0:
		return 0
	max_node_id=-1
	for nId in nodes_id:
		if g.node[nId]['color'] in allowed_colors:
			if max_node_id == -1 or \
			   weight(1,1,len(g[nId]),len(g[max_node_id]),nId,max_node_id) > 0:
				max_node_id = nId
	return max_node_id

def count_hops(g,nodeId):
	g.node[nodeId]['visited']=True
	nextNodeId=None
	for nId in g.neighbors(nodeId):
		if not 'visited' in g.node[nId] or not g.node[nId]['visited']:
			g.node[nId]['metric']=g.node[nodeId]['metric']+1
	for nId in g.neighbors(nodeId):
		if not 'visited' in g.node[nId] or not g.node[nId]['visited']:
			nextNodeId=nId
			break
	if nextNodeId==None:
		return g
	return count_hops(g,nextNodeId)
"""
implementation of centralized cds_bd_c2 algorithm, 
described in the paper: http://www.cs.gsu.edu/yli/papers/TPDS09.pdf

Little discription:

neigbhor coloring:
0 - white
1 - gray
2 - black

We are iterating over the graph verteces, starting with the hop == 0 
on each iteration we select verteces that are away from the leader on the distance == hop

1. Iterating over these verteces:
Select the vertex that has white color and max weight
Color it to black and its neighbors to gray
continue if there are more white verteces on the distance == hop

2. Connecting CDS.
Iterate over the hop distance starting with 1
On each iteration select the neighbors with the distance == hop - 1
if there are no black neighbors on hop-1, add neighbors with the highest weight

"""

def cds_bd_c2(g):
	# TODO: select leader with appropriate algorithm
	def color_neighbors(ids,color_from,color_to):
		for i in ids:
			if g.node[i]['color'] == color_from:
				g.node[i]['color'] = color_to
	leader=1

	for nId in g.nodes():
		g.node[nId]['color']=0
	g.node[leader]['metric']=0
	g=count_hops(g,leader)
	maxHop=max([g.node[i]['metric'] for i in g.nodes()])
	get_nodes_by_degree = lambda degree: [x for x in g.nodes() if g.node[x]['metric'] == degree]
	# iterating over the graph verteces, starting with the
	# node that is 0 hops away from the leader
	for i in xrange(0,maxHop+1):
		# select nodes with depth i
		nodes_id=get_nodes_by_degree(i)
		# if we have only 1 node, we do not need to go to the while loop
		if len(nodes_id)==1 and g.node[nodes_id[0]]['color']==0:
			g.node[nodes_id[0]]['color'] = 2
			color_neighbors(list(g.neighbors(nodes_id[0])),0,1)
			continue
		#while we have white nodes with the hop == i
		while (len(list([x for x in nodes_id if g.node[x]['color'] == 0]))) > 0:
			# find node with the maximum weight
			max_node_id=max_weight_node(g,nodes_id,[0])
			#color it to black and its neighbors to the gray
			g.node[max_node_id]['color'] = 2
			color_neighbors(g.neighbors(max_node_id),0,1)

	#connecting vertices
	#if on the hop == i we have node that has no black neighbors on the hop == i-1 
	#we select the neigbhor of the hop == i-1 with the highest weight
	for i in xrange(1,maxHop+1):
		nbrs_by_color_and_hop = lambda node_ids,hop,allowed_colors: [x for x in node_ids \
							 if g.node[x]['color'] in allowed_colors and g.node[x]['metric']==hop] 
		nodes_id = nbrs_by_color_and_hop(g.nodes(),i,[2])
		for nId in nodes_id:
			black_nbrs = nbrs_by_color_and_hop(g.neighbors(i),i-1,[2])
			if len(black_nbrs) == 0:
				max_node_id = max_weight_node(g,[x for x in nbrs_by_color_and_hop(g.neighbors(nId),i-1,[0,1])],[0,1])
				if max_node_id == 0:
					print 'warning, graph is probably Directional'
					continue
				g.node[max_node_id]['color'] = 2
	cds_set = [x for x in g.nodes() if g.node[x]['color'] == 2]
	return cds_set



g=nx.DiGraph()
g.add_edges_from([(1,2),(1,3),(1,6),(1,7),(2,4),(4,5),(5,6),(6,7),(6,8),(7,9),(8,9),\
	              (2,1),(3,1),(6,1),(7,1),(4,2),(5,4),(6,5),(7,6),(8,6),(9,7),(9,8)])

cds_set = cds_bd_c2(g)
for i in g.nodes():
	print i,g.node[i]
print cds_set
