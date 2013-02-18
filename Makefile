all:
	make mac

mac:
	CXX=g++ node-gyp configure
	CXX=g++ node-gyp build
	ln -s build/Release/nodetcl.node nodetcl.node

clean:
	node-gyp clean
	rm nodetcl.node
