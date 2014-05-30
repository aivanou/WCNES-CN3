import sys
import networkx as nx
import matplotlib.pyplot as plt


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
	max_node_id=0
	for nId in nodes_id:
		if g.node[nId]['color'] in allowed_colors:
			if max_node_id == 0 or \
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
	def color_neighbors(ids,color_from,color_to):
		for i in ids:
			if g.node[i]['color'] == color_from:
				g.node[i]['color'] = color_to
	# TODO: select leader with appropriate algorithm
	leader=1

	for nId in g.nodes():
		g.node[nId]['color']=0
	g.node[leader]['metric']=0
	g=count_hops(g,leader)
	maxHop=max([g.node[i]['metric'] for i in g.nodes()])
	get_nodes_by_state = lambda nodes,state,value: [x for x in nodes if g.node[x][state] == value]
	get_nodes_by_degree = lambda nodes,degree: get_nodes_by_state(nodes ,'metric', degree)
	get_nodes_by_color = lambda nodes,color: get_nodes_by_state(nodes ,'color', color)
	# iterating over the graph verteces, starting with the
	# node that is 0 hops away from the leader
	for i in xrange(0,maxHop+1):
		# select nodes with depth i
		nodes_id=get_nodes_by_degree(g.nodes(),i)
		# if we have only 1 node, we do not need to go to the while loop
		print i,nodes_id,g.node[nodes_id[0]]['color']
		if len(nodes_id)==1 and g.node[nodes_id[0]]['color']==0:
			g.node[nodes_id[0]]['color'] = 2
			color_neighbors(list(g.neighbors(nodes_id[0])),0,1)
			continue
		#while we have white nodes with the hop == i
		while (len(get_nodes_by_color(nodes_id,0))) > 0:
			# find node with the maximum weight
			max_node_id=max_weight_node(g,nodes_id,[0])
			#color it to black and its neighbors to the gray
			g.node[max_node_id]['color'] = 2
			color_neighbors(g.neighbors(max_node_id),0,1)

	#connecting vertices
	#if on the hop == i we have node that has no black neighbors on the hop == i-1 
	#we select the neigbhor of the hop == i-1 with the highest weight
	get_nodes_by_color_and_hop = lambda node_ids,hop,allowed_colors: [x for x in node_ids \
							 if g.node[x]['color'] in allowed_colors and g.node[x]['metric']==hop] 
	for i in xrange(1,maxHop+1):
		nodes_id = get_nodes_by_color_and_hop(g.nodes(),i,[2])
		for nId in nodes_id:
			black_nbrs = get_nodes_by_color_and_hop(g.neighbors(i),i-1,[2])
			if len(black_nbrs) == 0:
				# print nId,g.neighbors(nId)
				max_node_id = max_weight_node(g,get_nodes_by_color_and_hop(g.neighbors(nId),i-1,[0,1]),[0,1])
				if max_node_id == 0:
					print 'warning, graph is probably Directional'
					continue
				g.node[max_node_id]['color'] = 2
				g.node[nId]['color'] = 1
	cds_set = [x for x in g.nodes() if g.node[x]['color'] == 2]
	return cds_set


def is_k_connected(g,nodes,k):
	for nId in nodes:
		black_nbrs = [x for x in g.neighbors(nId) if g.node[x]['color'] == 2]
		if len(black_nbrs) < k \
			and len(g.neighbors(nId)) != len(black_nbrs):
			return False
	return True

"""
Centralized algorithm for building kmCDS
described in http://cs.gsu.edu/~yli/papers/mobihoc08.pdf
"""

def icga(g,k,m):
	get_nodes_by_state = lambda nodes,state,value: [x for x in nodes if g.node[x][state] == value]
	get_nodes_by_degree = lambda nodes,degree: get_nodes_by_state(nodes ,'metric', degree)
	get_nodes_by_color = lambda nodes,color: get_nodes_by_state(nodes ,'color', color)
	
	cds_set = cds_bd_c2(g)
	for nId in g.nodes():
		g.node[nId]['visited']=False
		if not g.node[nId]['color'] == 2:
			g.node[nId]['color'] = 0

	while True:
		white_nodes = get_nodes_by_color(g.nodes(),0)
		stop=True
		for nId in white_nodes:
			bn = get_nodes_by_color(g.neighbors(nId),2)
			if len(bn) < m:
				stop=False
		if stop: 
			break
		wn = [x for x in white_nodes if len(get_nodes_by_color(g.neighbors(x),2)) < m]
		m_id = max([(get_nodes_by_color(g.neighbors(x),0),x) for x in wn])[1]
		g.node[m_id]['color'] = 2


	for i in xrange(1,k+1):
		while not is_k_connected(g,get_nodes_by_color(g.nodes(),2),i):
			node_id = max_weight_node(g,get_nodes_by_color(g.nodes(),0),[0,1])
			if node_id == 0:
				print 'warning, cannot find new node'
				return				
			g.node[node_id]['color']=2
	for nId in g.nodes():
		g.node[nId]['weight'] = g.node[nId]['color']


def drawGraph(G, title, n):
	# Find the sub-optimal edges
	pos = nx.spring_layout(G) # positions for all nodes
	plt.subplot(220 + n)
	plt.title(title)
	plt.axis('off')
	values=[G.node[n]['color'] for n in G.nodes()]
	nx.draw(g, cmap = plt.get_cmap('jet'), node_color = values)
	

g=nx.DiGraph()
g.add_edges_from([(1,2),(1,3),(1,6),(1,7),(2,4),(4,5),(5,6),(6,7),(6,8),(7,9),(8,9),(9,10),\
	              (2,1),(3,1),(6,1),(7,1),(4,2),(5,4),(6,5),(7,6),(8,6),(9,7),(9,8),(10,9)])

for n in g.nodes():
	g.node[n]['color']=0


cds_set = cds_bd_c2(g)

drawGraph(g,"CDS_BD_C2",1)

# for n in g.nodes():
# 	g.node[n]['color']=0

# icga(g,2,1)

# drawGraph(g,"ICGA",2)
# plt.show()
# cds_set = cds_bd_c2(g)
for i in g.nodes():
	print i,g.node[i]
print cds_set

