#/usr/bin/python2.7
import re
import networkx as nx
import matplotlib.pyplot as plt

def removeEdgesFromNode(G, edges, node):
	for (u, v, d) in edges:
		if u == node:
			edges.remove((u, v, d))

def extractMax(G, reliability, nodes):
	maxVal = 0
	child = None
	parent = None
	for u, v, data in G.edges(data=True):
		if u in nodes and not v in nodes:
			if v == '1.0':
				print v

			r = data['weight']
			if reliability[v] * r > reliability[u] and reliability[v] * r > maxVal:
				maxVal = reliability[v] * r
				child = u
				parent = v
	return (child, parent, maxVal)

def extractMaxEtx(etxValues):
	maxVal = 0
	child = None
	parent = None

def primEtx(etxValues, sink):
	nodes = [n for n in etxValues if n != sink]
	edges = {}
	# Reliability indicates the reliability from a node to the sink
	# A reliability of 1.0 indicates a 100 % good link while 0 is 
	# an awful link.
	reliability = dict([(n, 0) for n in etxValues])
	etxValues[sink] = 1
	mst = nx.Graph()

	while len(etxValues) > 0:
		(u, v, r) = extractMaxEtx(etxValues)
		if u != None:
			if u in nodes:
				nodes.remove(u)
			reliability[u] = r
			edges[u] = (u, v, r / reliability[v])
		else:
			break
	for node in nodes:
		mst.add_node(node)
	for u, v, r in edges:
		mst.add_edge(u, v, weight = r)
	return mst


def prim(G, sink):
	nodes = [n for n in G.nodes() if n != sink]
	edges = []
	# Reliability indicates the reliability from a node to the sink
	# A reliability of 1.0 indicates a 100 % good link while 0 is 
	# an awful link.
	reliability = dict([(n, 0) for n in G.nodes()])
	reliability[sink] = 1
	mst = nx.Graph()

	while len(nodes) > 0:
		(u, v, r) = extractMax(G, reliability, nodes)
		if u != None:
			if u in nodes:
				nodes.remove(u)
			reliability[u] = r
			edges.append((u, v, r / reliability[v]))
		else:
			break
	for node in G.nodes():
		mst.add_node(node)
	for u, v, r in edges:
		mst.add_edge(u, v, weight = r)
	return mst

# The networkx "arrows" for the edges are just thicker were they end and are not really arrows.
# The weight of the edges are outputted close to the node they go from, the originating node.
def drawGraph(G, title, n):
	# Find the sub-optimal edges
	pos = nx.spring_layout(G) # positions for all nodes
	plt.subplot(220 + n)
	plt.title(title)
	plt.axis('off')
	nx.draw_networkx_labels(G, pos, font_size = 10, font_family = 'sans-serif')
	# nodes
	nx.draw_networkx_nodes(G, pos, cmap = plt.get_cmap('jet'), node_size=350)
	# edges
	nx.draw_networkx_edges(G, pos, edge_color = 'black', arrows = True)
	# edge labels
	edge_labels=dict([((u,v,),"%.2f" % float(d['weight'])) for u,v,d in G.edges(data=True)])

	nx.draw_networkx_edge_labels(G, pos, edge_labels=edge_labels, label_pos=0.75, font_color='black')
	

def normalizeData(d, negative = False):
	maxVal = None
	minVal = None
	for index in d:
		val = d[index]
		#Find minimum and maximum rssi and lqi values
		if maxVal == None or (negative and maxVal < val) or (not negative and maxVal < val):
			maxVal = val
		if minVal == None or (negative and minVal > val) or (not negative and minVal > val):
			minVal = val

	normalizedData = dict([(n, d[n]) for n in d])
	if negative:
		diff = abs(maxVal) + abs(minVal)
		for index in d:
			val = d[index]
			normalizedData[index] = (diff + val) / abs(minVal)
	else:
		for index in d:
			normalizedData[index] = val / maxVal


	#for index in d:
	#	val = d[index]
	#	#Changed normalization to work correctly with negative values
	#	normalizedData[index] = (val - minVal) / (maxVal - minVal)
	return normalizedData


def parseFile(fileName):
	f = open(fileName, 'r')
	rssi = {}
	lqi = {}
	node_id={};
	etx = {}
	sink = 1
	u = 0
	v = 0
	for line in f:
		data = re.findall("[A-Za-z:,\ ]*([\-]*[0-9\.]+)", line)
		if len(data) == 1:  # I am sink
			sink = data[0]
		elif len(data) == 2: # ETX
			u = data[0]
			etx[u] = float(data[1])
		elif len(data) == 7: # Else RSSI and LQI
			u = data[0]
			v = data[1]
			# data[2] is total amount of packets
			# data[3] is packets lost
			lqi[(u, v)] = float(data[4])
			rssi[(u, v)] = float(data[5])
	
	return (sink, rssi, lqi, etx)

if __name__ == "__main__":
	rssiGraph = nx.DiGraph() # The graph holding data about RSSI
	rssiBestEdges = []       # The optimal RSSI edge for each node
	lqiGraph = nx.DiGraph()  # The graph holding data about LQI
	lqiBestEdges = []        # The optimal LQI edge for each node
	(sink, rssi, lqi, etx) = parseFile("graph.txt")

	#print('\n')
	#print(rssi)
	#print('\n')
	#print(lqi)

	normLqi = normalizeData(lqi)
	normRssi = normalizeData(rssi, True)

	for (u, v) in normLqi:
		if u not in lqiGraph.nodes():
			lqiGraph.add_node(u)
		if v not in lqiGraph.nodes():
			lqiGraph.add_node(v)
		lqiGraph.add_edge(u, v, weight = normLqi[(u, v)])

	for (u, v) in normRssi:
		if u not in rssiGraph.nodes():
			rssiGraph.add_node(u)
		if v not in rssiGraph.nodes():
			rssiGraph.add_node(v)
		rssiGraph.add_edge(u, v, weight = normRssi[(u, v)])

	drawGraph(rssiGraph, 'RSSI Graph', 1)
	drawGraph(lqiGraph, 'LQI Graph', 3)


	bestRssiMST = None
	bestRssiVal = 0
	bestRssiSink = None
	for sink in rssiGraph.nodes():
		rssiMst = prim(rssiGraph, sink)
		rssiVal = 0
		for u, v, data in rssiMst.edges(data=True):
			rssiVal += data['weight']
		if rssiVal > bestRssiVal:
			bestRssiMst = rssiMst
			bestRssiVal = rssiVal
			bestRssiSink = sink

	bestLqiMST = None
	bestLqiVal = 0
	bestLqiSink = None
	for sink in lqiGraph.nodes():
		lqiMst = prim(lqiGraph, sink)
		lqiVal = 0
		for u, v, data in lqiMst.edges(data=True):
			lqiVal += data['weight']
		if lqiVal > bestLqiVal:
			bestLqiMst = lqiMst
			bestLqiVal = lqiVal
			bestLqiSink = sink

	print "Best RSSI sink: " + bestRssiSink
	print "Best LQI sink: " + bestLqiSink
	drawGraph(rssiMst, 'RSSI MST', 2)
	drawGraph(lqiMst, 'LQI MST', 4)

	#Calculate the betweeness centrality of the nodes.
	BC_rssi = nx.betweenness_centrality(rssiGraph,None,False,'normRssi')        
	BC_lqi = nx.betweenness_centrality(lqiGraph,None,False,'normLqi')

	#print "Betweeness centrality nodes ranking, of RSSI graph"
	print(BC_rssi)
	#print('\nBetweeness centrality nodes ranking, of LQI graph\n')
	#print(BC_lqi)
	plt.show()
	
