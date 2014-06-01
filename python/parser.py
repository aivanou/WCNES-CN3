#/usr/bin/python2.7
import re,sys
import networkx as nx
from pylab import *
import matplotlib.pyplot as plt

# Returns the maximum distance over an array of vertices, V
# given the distances from the sink of a graph
def extractMax(V, distances):
	maxVal = 0
	node = None
	for u in V:
		if distances[u] > maxVal:
			maxVal = distances[u]
			node = u
	if node != None:
		# Remove the vertex
		V.remove(node)
	return node


# Returns the minimum spanning tree of the tree G with the given sink
# If the number of edges in the resulting MST is less than the number 
# of nodes None will be returned.
def prim(G, sink):
	# The unvisited vertices
	V = [n for n in G.nodes()]
	distances = dict([(n, -100000.0) for n in G.nodes()]) # V
	parents = dict([(n, None) for n in G.nodes()])
	distances[sink] = 1.0 # The distance as reliability
	while len(V) > 0:
		# Get the node with the lowest value from the unvisited vertices
		u = extractMax(V, distances)
		if u != None:
			# Get all edges to the edge u.
			for v, parent, data in G.edges(data=True):
				if parent == u:
					d = data['weight'] * distances[u]
					# If the distance is better than before, update it and the 
					# parent of the node
					if v in V and d > distances[v]:
						distances[v] = d
						parents[v] = u
		else:
			# an error occured
			break
	# All nodes, but the sink, must have a parent node.
	if parents.values().count(None) > 1:
		return None
	else:
		# Greate the actial graph
		mst = nx.Graph()
		# Add the vertices
		for node in G.nodes():
			mst.add_node(node)
		# Add the edges
		for v, u, data in G.edges(data=True):
			if parents[v] == u:
				mst.add_edge(v, u, weight = data['weight'])
		return mst

# The networkx "arrows" for the edges are just thicker were they end and are not really arrows.
# The weight of the edges are outputted close to the node they go from, the originating node.
def drawGraph(G, title, n):
	# Find the sub-optimal edges
	pos = nx.spring_layout(G) # positions for all nodes
	#plt.subplot(220 + n)
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
	

def normalizeData(d):
	# Create a new dict for holding the normalized data
	normalizedData = dict([(n, d[n]) for n in d])
	# Get the maximum and minimum values
	maxVal = max(d.values())
	minVal = min(d.values())
	
	# If negative values are inputted then 
	if minVal < 0:
		# Example: -3 is max and -50:
		# diff = 53
		# - 3 -> (53 + (- 3)) / 50 = 50 / 50 = 1.0
		# -50 -> (53 + (-50)) / 50 = 50 /  3 = 0.06
		diff = abs(maxVal) + abs(minVal)
		for index in d:
			val = d[index]
			normalizedData[index] = (diff + val) / abs(minVal)
	else:
		# Standard normalization
		for index in d:
			val = d[index]
			normalizedData[index] = val / maxVal
	# Return the normalized array
	return normalizedData


def parseFile(fileName):
	print "parsing file ", fileName
	f = open(fileName, 'r')
	rssi = {}
	lqi = {}
	node_id={};
	etx = {}
	sink = 75
	u = 0
	v = 0
	for line in f:
		data = re.findall("[A-Za-z:,\ ]*([\-]*[0-9\.]+)", line)
		#print len(data)
		if len(data) != 9:
			continue
		data = [d for d in data if len(d)>0]
		u = data[0].strip()
		v = data[1].strip()
		lqi[(u, v)] = float(data[5])
		rssi[(u, v)] = float(data[6])
		etx[(u, v)] = float(data[7])
	
	return ("75.0", rssi, lqi, etx)

if __name__ == "__main__":


	fname = "graph.txt"
	if( len(sys.argv) > 1):
		fname = sys.argv[1]

	(sink, rssi, lqi, etx) = parseFile(fname)

	# Normalize the lqi and rssi values.
	normLqi = normalizeData(lqi)
	normRssi = normalizeData(rssi)

	# Create the RSSI graph
	rssiGraph = nx.DiGraph() # The graph holding data about RSSI
	for u, v in normRssi:
		if u not in rssiGraph.nodes():
			rssiGraph.add_node(u)
		if v not in rssiGraph.nodes():
			rssiGraph.add_node(v)
		rssiGraph.add_edge(u, v, weight = normRssi[(u, v)])

	# Create the LQI Graph
	lqiGraph = nx.DiGraph()  # The graph holding data about LQI
	for u, v in normLqi:
		if u not in lqiGraph.nodes():
			lqiGraph.add_node(u)
		if v not in lqiGraph.nodes():
			lqiGraph.add_node(v)
		lqiGraph.add_edge(u, v, weight = normLqi[(u, v)])

	# Create the ETX Graph
	etxGraph = nx.DiGraph()  # The graph holding data about ETX
	for u, v in etx:
		if u not in etxGraph.nodes():
			etxGraph.add_node(u)
		if v not in etxGraph.nodes():
			etxGraph.add_node(v)
		etxGraph.add_edge(u, v, weight = etx[(u, v)])

	bestRssiMst = None
	bestRssiVal = 0
	bestRssiSink = None
	for sink in rssiGraph.nodes():
		mst = prim(rssiGraph, sink)
		if mst != None:
			val = 0
			for u, v, data in mst.edges(data=True):
				val += data['weight']
			if val > bestRssiVal:
				bestRssiMst = mst
				bestRssiVal = val
				bestRssiSink = sink

	bestLqiMst = None
	bestLqiVal = 0
	bestLqiSink = None
	for sink in lqiGraph.nodes():
		mst = prim(lqiGraph, sink)
		if mst == None:
			continue
		else:
			val = 0
			for u, v, data in mst.edges(data=True):
				val += data['weight']
			if val > bestLqiVal:
				bestLqiMst = mst
				bestLqiVal = val
				bestLqiSink = sink

	bestEtxMst = None
	bestEtxVal = 0
	bestEtxSink = None
	for sink in etxGraph.nodes():
		mst = prim(etxGraph, sink)
		if mst == None:
			continue
		else:
			val = 0
			for u, v, data in mst.edges(data=True):
				val += data['weight']
			if val > bestEtxVal:
				bestEtxMst = mst
				bestEtxVal = val
				bestEtxSink = sink

	print "Best RSSI sink: " , bestRssiSink
	print "Best LQI sink: " , bestLqiSink
	print "Best ETX sink: " , bestEtxSink

	# Draw the graphs and the MSTs
	plt.subplot(231)
	drawGraph(rssiGraph, 'RSSI Graph', 0)
	plt.subplot(232)
	drawGraph(lqiGraph, 'LQI Graph', 0)
	plt.subplot(233)
	drawGraph(etxGraph, 'ETX Graph', 0)

	if bestRssiMst != None:
		plt.subplot(234)
		drawGraph(bestRssiMst, 'RSSI MST', 0)
	if bestLqiMst != None:
		plt.subplot(235)
		drawGraph(bestLqiMst, 'LQI MST', 0)
	if bestEtxMst != None:
		plt.subplot(236)
		drawGraph(bestEtxMst, 'ETX MST', 0)

	#Calculate the betweeness centrality of the nodes.
	#BC_rssi = nx.betweenness_centrality(rssiGraph,None,False,'normRssi')        
	BC_lqi = nx.betweenness_centrality(lqiGraph,None,False,'normLqi')

	#print "Betweeness centrality nodes ranking, of RSSI graph"
	#print(BC_rssi)
	#print('\nBetweeness centrality nodes ranking, of LQI graph\n')
	#print(BC_lqi)
	plt.show()
	
