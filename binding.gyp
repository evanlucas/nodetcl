{
  "targets": [
    {
      "target_name": "nodetcl",
      "sources": ["src/nodetcl.cc"],
      "include_dirs": [
        "<!(node -e \"require('nan')\")"
      ],
      "conditions": [
        ['OS=="mac"', {
          "defines": [ '__MACOSX_CORE__' ],
          'libraries': [ '-ltcl' ],
          'xcode_settings': {
            'OTHER_CFLAGS': [
              '-fno-exceptions'
            ]
          }
        }]
      ]
    }
  ]
}
