const bindings = require('bindings')('fast-ssdeep.node');

delete bindings.path; //bindings lib adds this

module.exports = bindings;