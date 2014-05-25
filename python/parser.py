#/usr/bin/python2.7
import re
import networkx as nx
import matplotlib.pyplot as plt

def extractMax(V, distances):
	maxVal = 0
	node = None
	for u in V:
		if distances[u] > maxVal:
			maxVal = distances[u]
			node = u
	return node

def prim(G, sink):
	# The unvisited vertices
	V = [n for n in G.nodes()]
	distances = dict([(n, 0.0) for n in G.nodes()]) # V
	parents = dict([(n, None) for n in G.nodes()])
	distances[sink] = 1.0
	# Reliability indicates the reliability from a node to the sink
	# A reliability of 1.0 indicates a 100 % good link while 0 is 
	# an awful link.
	while len(V) > 0:
		u = extractMax(V, distances)
		if u != None:
			V.remove(u)
			for v, parent, data in G.edges(data=True):
				if parent == u:
					d = data['weight'] * distances[u]
					if v in V and d > distances[v]:
						distances[v] = d
						parents[v] = u
		else:
			break
	if parents.values().count(None) > 1:
		return None
	else:
		mst = nx.Graph()
		for node in G.nodes():
			mst.add_node(node)
		for v, u, data in G.edges(data=True):
			if parents[v] == u:
				mst.add_edge(v, u, weight = data['weight'])
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


	bestRssiMst = None
	bestRssiVal = 0
	bestRssiSink = None
	for sink in rssiGraph.nodes():
		rssiMst = prim(rssiGraph, sink)
		if rssiMst == None:
			continue
		else:
			rssiVal = 0
			for u, v, data in rssiMst.edges(data=True):
				rssiVal += data['weight']
			if rssiVal > bestRssiVal:
				bestRssiMst = rssiMst
				bestRssiVal = rssiVal
				bestRssiSink = sink

	bestLqiMst = None
	bestLqiVal = 0
	bestLqiSink = None
	for sink in lqiGraph.nodes():
		lqiMst = prim(lqiGraph, sink)
		if lqiMst == None:
			continue
		else:
			lqiVal = 0
			for u, v, data in lqiMst.edges(data=True):
				lqiVal += data['weight']
			if lqiVal > bestLqiVal:
				bestLqiMst = lqiMst
				bestLqiVal = lqiVal
				bestLqiSink = sink

	print "Best RSSI sink: " + bestRssiSink
	print "Best LQI sink: " + bestLqiSink
	if bestRssiMst != None:
		drawGraph(bestRssiMst, 'RSSI MST', 2)
	if bestLqiMst != None:
		drawGraph(bestLqiMst, 'LQI MST', 4)

	#Calculate the betweeness centrality of the nodes.
	BC_rssi = nx.betweenness_centrality(rssiGraph,None,False,'normRssi')        
	BC_lqi = nx.betweenness_centrality(lqiGraph,None,False,'normLqi')

	#print "Betweeness centrality nodes ranking, of RSSI graph"
	print(BC_rssi)
	#print('\nBetweeness centrality nodes ranking, of LQI graph\n')
	#print(BC_lqi)
	plt.show()
	
