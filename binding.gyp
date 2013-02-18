{
	"targets": [
		{
			"target_name": "nodetcl",
			"sources": ["src/nodetcl.cc"],
			"conditions": [
				['OS=="mac"', {
					"defines": [ '__MACOSX_CORE__' ],
					'ldflags': [ '-libtcl8.5' ],
					"link_settings": {
						"libraries": [ '/usr/lib/libtcl8.5.dylib' ]
					}
				}]
			]
		}
	]
}
