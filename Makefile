all:
	make mac

mac:
	node-gyp configure
	node-gyp build
	ln -s build/Release/nodetcl.node nodetcl.node

clean:
	node-gyp clean
	rm nodetcl.node
