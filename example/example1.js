//
// Demonstrates how to create a Tcl interpreter and then invoke a
// simple Tcl command from JavaScript.
//

var tcl = require('../')
var interp = new tcl()

var args = process.argv.splice(2)
if (args.length < 2) {
  throw new Error('At least two args are required')
}

console.log(interp.eval(`expr ${args[0]}*${args[1]}`))
