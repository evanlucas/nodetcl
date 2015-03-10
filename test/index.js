var test = require('tap').test
  , TCL = require('../')

test('eval', function(t) {
  t.plan(1)
  var interp = new TCL()
  t.equal(42, interp.eval('expr 7*6'), '7*6 should equal 42')
})

test('proc', function(t) {
  t.plan(3)
  var interp = new TCL()
  interp.proc('cb', function(arg1, arg2) {
    t.equal('one', arg1, 'arg1 should be `one`')
    t.equal('two', arg2, 'arg2 should be `two`')
    return 'result'
  })

  var res = interp.eval('cb one two')
  t.equal(res, 'result', 'result should be `result`')
})

test('call', function(t) {
  t.plan(1)
  var interp = new TCL()
  t.equal(5, interp.call('llength', [1, 2, 3, 4, 5]), 'array len should be 5')
})
