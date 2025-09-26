'use strict';

var require$$0 = require('path');
var require$$2 = require('util');
var require$$0$1 = require('fs');
var require$$3 = require('events');
var require$$5 = require('assert');
var require$$0$2 = require('os');
var require$$1 = require('tty');
var require$$1$1 = require('url');
var require$$12 = require('module');

var commonjsGlobal = typeof globalThis !== 'undefined' ? globalThis : typeof window !== 'undefined' ? window : typeof global !== 'undefined' ? global : typeof self !== 'undefined' ? self : {};

function getAugmentedNamespace(n) {
  var f = n.default;
	if (typeof f == "function") {
		var a = function () {
			return f.apply(this, arguments);
		};
		a.prototype = f.prototype;
  } else a = {};
  Object.defineProperty(a, '__esModule', {value: true});
	Object.keys(n).forEach(function (k) {
		var d = Object.getOwnPropertyDescriptor(n, k);
		Object.defineProperty(a, k, d.get ? d : {
			enumerable: true,
			get: function () {
				return n[k];
			}
		});
	});
	return a;
}

var old$1 = {};

// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var pathModule = require$$0;
var isWindows = process.platform === 'win32';
var fs$5 = require$$0$1;

// JavaScript implementation of realpath, ported from node pre-v6

var DEBUG = process.env.NODE_DEBUG && /fs/.test(process.env.NODE_DEBUG);

function rethrow() {
  // Only enable in debug mode. A backtrace uses ~1000 bytes of heap space and
  // is fairly slow to generate.
  var callback;
  if (DEBUG) {
    var backtrace = new Error;
    callback = debugCallback;
  } else
    callback = missingCallback;

  return callback;

  function debugCallback(err) {
    if (err) {
      backtrace.message = err.message;
      err = backtrace;
      missingCallback(err);
    }
  }

  function missingCallback(err) {
    if (err) {
      if (process.throwDeprecation)
        throw err;  // Forgot a callback but don't know where? Use NODE_DEBUG=fs
      else if (!process.noDeprecation) {
        var msg = 'fs: missing callback ' + (err.stack || err.message);
        if (process.traceDeprecation)
          console.trace(msg);
        else
          console.error(msg);
      }
    }
  }
}

function maybeCallback(cb) {
  return typeof cb === 'function' ? cb : rethrow();
}

pathModule.normalize;

// Regexp that finds the next partion of a (partial) path
// result is [base_with_slash, base], e.g. ['somedir/', 'somedir']
if (isWindows) {
  var nextPartRe = /(.*?)(?:[\/\\]+|$)/g;
} else {
  var nextPartRe = /(.*?)(?:[\/]+|$)/g;
}

// Regex to find the device root, including trailing slash. E.g. 'c:\\'.
if (isWindows) {
  var splitRootRe = /^(?:[a-zA-Z]:|[\\\/]{2}[^\\\/]+[\\\/][^\\\/]+)?[\\\/]*/;
} else {
  var splitRootRe = /^[\/]*/;
}

old$1.realpathSync = function realpathSync(p, cache) {
  // make p is absolute
  p = pathModule.resolve(p);

  if (cache && Object.prototype.hasOwnProperty.call(cache, p)) {
    return cache[p];
  }

  var original = p,
      seenLinks = {},
      knownHard = {};

  // current character position in p
  var pos;
  // the partial path so far, including a trailing slash if any
  var current;
  // the partial path without a trailing slash (except when pointing at a root)
  var base;
  // the partial path scanned in the previous round, with slash
  var previous;

  start();

  function start() {
    // Skip over roots
    var m = splitRootRe.exec(p);
    pos = m[0].length;
    current = m[0];
    base = m[0];
    previous = '';

    // On windows, check that the root exists. On unix there is no need.
    if (isWindows && !knownHard[base]) {
      fs$5.lstatSync(base);
      knownHard[base] = true;
    }
  }

  // walk down the path, swapping out linked pathparts for their real
  // values
  // NB: p.length changes.
  while (pos < p.length) {
    // find the next part
    nextPartRe.lastIndex = pos;
    var result = nextPartRe.exec(p);
    previous = current;
    current += result[0];
    base = previous + result[1];
    pos = nextPartRe.lastIndex;

    // continue if not a symlink
    if (knownHard[base] || (cache && cache[base] === base)) {
      continue;
    }

    var resolvedLink;
    if (cache && Object.prototype.hasOwnProperty.call(cache, base)) {
      // some known symbolic link.  no need to stat again.
      resolvedLink = cache[base];
    } else {
      var stat = fs$5.lstatSync(base);
      if (!stat.isSymbolicLink()) {
        knownHard[base] = true;
        if (cache) cache[base] = base;
        continue;
      }

      // read the link if it wasn't read before
      // dev/ino always return 0 on windows, so skip the check.
      var linkTarget = null;
      if (!isWindows) {
        var id = stat.dev.toString(32) + ':' + stat.ino.toString(32);
        if (seenLinks.hasOwnProperty(id)) {
          linkTarget = seenLinks[id];
        }
      }
      if (linkTarget === null) {
        fs$5.statSync(base);
        linkTarget = fs$5.readlinkSync(base);
      }
      resolvedLink = pathModule.resolve(previous, linkTarget);
      // track this, if given a cache.
      if (cache) cache[base] = resolvedLink;
      if (!isWindows) seenLinks[id] = linkTarget;
    }

    // resolve the link, then start over
    p = pathModule.resolve(resolvedLink, p.slice(pos));
    start();
  }

  if (cache) cache[original] = p;

  return p;
};


old$1.realpath = function realpath(p, cache, cb) {
  if (typeof cb !== 'function') {
    cb = maybeCallback(cache);
    cache = null;
  }

  // make p is absolute
  p = pathModule.resolve(p);

  if (cache && Object.prototype.hasOwnProperty.call(cache, p)) {
    return process.nextTick(cb.bind(null, null, cache[p]));
  }

  var original = p,
      seenLinks = {},
      knownHard = {};

  // current character position in p
  var pos;
  // the partial path so far, including a trailing slash if any
  var current;
  // the partial path without a trailing slash (except when pointing at a root)
  var base;
  // the partial path scanned in the previous round, with slash
  var previous;

  start();

  function start() {
    // Skip over roots
    var m = splitRootRe.exec(p);
    pos = m[0].length;
    current = m[0];
    base = m[0];
    previous = '';

    // On windows, check that the root exists. On unix there is no need.
    if (isWindows && !knownHard[base]) {
      fs$5.lstat(base, function(err) {
        if (err) return cb(err);
        knownHard[base] = true;
        LOOP();
      });
    } else {
      process.nextTick(LOOP);
    }
  }

  // walk down the path, swapping out linked pathparts for their real
  // values
  function LOOP() {
    // stop if scanned past end of path
    if (pos >= p.length) {
      if (cache) cache[original] = p;
      return cb(null, p);
    }

    // find the next part
    nextPartRe.lastIndex = pos;
    var result = nextPartRe.exec(p);
    previous = current;
    current += result[0];
    base = previous + result[1];
    pos = nextPartRe.lastIndex;

    // continue if not a symlink
    if (knownHard[base] || (cache && cache[base] === base)) {
      return process.nextTick(LOOP);
    }

    if (cache && Object.prototype.hasOwnProperty.call(cache, base)) {
      // known symbolic link.  no need to stat again.
      return gotResolvedLink(cache[base]);
    }

    return fs$5.lstat(base, gotStat);
  }

  function gotStat(err, stat) {
    if (err) return cb(err);

    // if not a symlink, skip to the next path part
    if (!stat.isSymbolicLink()) {
      knownHard[base] = true;
      if (cache) cache[base] = base;
      return process.nextTick(LOOP);
    }

    // stat & read the link if not read before
    // call gotTarget as soon as the link target is known
    // dev/ino always return 0 on windows, so skip the check.
    if (!isWindows) {
      var id = stat.dev.toString(32) + ':' + stat.ino.toString(32);
      if (seenLinks.hasOwnProperty(id)) {
        return gotTarget(null, seenLinks[id], base);
      }
    }
    fs$5.stat(base, function(err) {
      if (err) return cb(err);

      fs$5.readlink(base, function(err, target) {
        if (!isWindows) seenLinks[id] = target;
        gotTarget(err, target);
      });
    });
  }

  function gotTarget(err, target, base) {
    if (err) return cb(err);

    var resolvedLink = pathModule.resolve(previous, target);
    if (cache) cache[base] = resolvedLink;
    gotResolvedLink(resolvedLink);
  }

  function gotResolvedLink(resolvedLink) {
    // resolve the link, then start over
    p = pathModule.resolve(resolvedLink, p.slice(pos));
    start();
  }
};

var fs_realpath = realpath;
realpath.realpath = realpath;
realpath.sync = realpathSync;
realpath.realpathSync = realpathSync;
realpath.monkeypatch = monkeypatch;
realpath.unmonkeypatch = unmonkeypatch;

var fs$4 = require$$0$1;
var origRealpath = fs$4.realpath;
var origRealpathSync = fs$4.realpathSync;

var version$1 = process.version;
var ok = /^v[0-5]\./.test(version$1);
var old = old$1;

function newError (er) {
  return er && er.syscall === 'realpath' && (
    er.code === 'ELOOP' ||
    er.code === 'ENOMEM' ||
    er.code === 'ENAMETOOLONG'
  )
}

function realpath (p, cache, cb) {
  if (ok) {
    return origRealpath(p, cache, cb)
  }

  if (typeof cache === 'function') {
    cb = cache;
    cache = null;
  }
  origRealpath(p, cache, function (er, result) {
    if (newError(er)) {
      old.realpath(p, cache, cb);
    } else {
      cb(er, result);
    }
  });
}

function realpathSync (p, cache) {
  if (ok) {
    return origRealpathSync(p, cache)
  }

  try {
    return origRealpathSync(p, cache)
  } catch (er) {
    if (newError(er)) {
      return old.realpathSync(p, cache)
    } else {
      throw er
    }
  }
}

function monkeypatch () {
  fs$4.realpath = realpath;
  fs$4.realpathSync = realpathSync;
}

function unmonkeypatch () {
  fs$4.realpath = origRealpath;
  fs$4.realpathSync = origRealpathSync;
}

var concatMap$1 = function (xs, fn) {
    var res = [];
    for (var i = 0; i < xs.length; i++) {
        var x = fn(xs[i], i);
        if (isArray(x)) res.push.apply(res, x);
        else res.push(x);
    }
    return res;
};

var isArray = Array.isArray || function (xs) {
    return Object.prototype.toString.call(xs) === '[object Array]';
};

var balancedMatch = balanced$1;
function balanced$1(a, b, str) {
  if (a instanceof RegExp) a = maybeMatch(a, str);
  if (b instanceof RegExp) b = maybeMatch(b, str);

  var r = range$1(a, b, str);

  return r && {
    start: r[0],
    end: r[1],
    pre: str.slice(0, r[0]),
    body: str.slice(r[0] + a.length, r[1]),
    post: str.slice(r[1] + b.length)
  };
}

function maybeMatch(reg, str) {
  var m = str.match(reg);
  return m ? m[0] : null;
}

balanced$1.range = range$1;
function range$1(a, b, str) {
  var begs, beg, left, right, result;
  var ai = str.indexOf(a);
  var bi = str.indexOf(b, ai + 1);
  var i = ai;

  if (ai >= 0 && bi > 0) {
    if(a===b) {
      return [ai, bi];
    }
    begs = [];
    left = str.length;

    while (i >= 0 && !result) {
      if (i == ai) {
        begs.push(i);
        ai = str.indexOf(a, i + 1);
      } else if (begs.length == 1) {
        result = [ begs.pop(), bi ];
      } else {
        beg = begs.pop();
        if (beg < left) {
          left = beg;
          right = bi;
        }

        bi = str.indexOf(b, i + 1);
      }

      i = ai < bi && ai >= 0 ? ai : bi;
    }

    if (begs.length) {
      result = [ left, right ];
    }
  }

  return result;
}

var concatMap = concatMap$1;
var balanced = balancedMatch;

var braceExpansion = expandTop;

var escSlash = '\0SLASH'+Math.random()+'\0';
var escOpen = '\0OPEN'+Math.random()+'\0';
var escClose = '\0CLOSE'+Math.random()+'\0';
var escComma = '\0COMMA'+Math.random()+'\0';
var escPeriod = '\0PERIOD'+Math.random()+'\0';

function numeric(str) {
  return parseInt(str, 10) == str
    ? parseInt(str, 10)
    : str.charCodeAt(0);
}

function escapeBraces(str) {
  return str.split('\\\\').join(escSlash)
            .split('\\{').join(escOpen)
            .split('\\}').join(escClose)
            .split('\\,').join(escComma)
            .split('\\.').join(escPeriod);
}

function unescapeBraces(str) {
  return str.split(escSlash).join('\\')
            .split(escOpen).join('{')
            .split(escClose).join('}')
            .split(escComma).join(',')
            .split(escPeriod).join('.');
}


// Basically just str.split(","), but handling cases
// where we have nested braced sections, which should be
// treated as individual members, like {a,{b,c},d}
function parseCommaParts(str) {
  if (!str)
    return [''];

  var parts = [];
  var m = balanced('{', '}', str);

  if (!m)
    return str.split(',');

  var pre = m.pre;
  var body = m.body;
  var post = m.post;
  var p = pre.split(',');

  p[p.length-1] += '{' + body + '}';
  var postParts = parseCommaParts(post);
  if (post.length) {
    p[p.length-1] += postParts.shift();
    p.push.apply(p, postParts);
  }

  parts.push.apply(parts, p);

  return parts;
}

function expandTop(str) {
  if (!str)
    return [];

  // I don't know why Bash 4.3 does this, but it does.
  // Anything starting with {} will have the first two bytes preserved
  // but *only* at the top level, so {},a}b will not expand to anything,
  // but a{},b}c will be expanded to [a}c,abc].
  // One could argue that this is a bug in Bash, but since the goal of
  // this module is to match Bash's rules, we escape a leading {}
  if (str.substr(0, 2) === '{}') {
    str = '\\{\\}' + str.substr(2);
  }

  return expand$1(escapeBraces(str), true).map(unescapeBraces);
}

function embrace(str) {
  return '{' + str + '}';
}
function isPadded(el) {
  return /^-?0\d/.test(el);
}

function lte(i, y) {
  return i <= y;
}
function gte(i, y) {
  return i >= y;
}

function expand$1(str, isTop) {
  var expansions = [];

  var m = balanced('{', '}', str);
  if (!m || /\$$/.test(m.pre)) return [str];

  var isNumericSequence = /^-?\d+\.\.-?\d+(?:\.\.-?\d+)?$/.test(m.body);
  var isAlphaSequence = /^[a-zA-Z]\.\.[a-zA-Z](?:\.\.-?\d+)?$/.test(m.body);
  var isSequence = isNumericSequence || isAlphaSequence;
  var isOptions = m.body.indexOf(',') >= 0;
  if (!isSequence && !isOptions) {
    // {a},b}
    if (m.post.match(/,.*\}/)) {
      str = m.pre + '{' + m.body + escClose + m.post;
      return expand$1(str);
    }
    return [str];
  }

  var n;
  if (isSequence) {
    n = m.body.split(/\.\./);
  } else {
    n = parseCommaParts(m.body);
    if (n.length === 1) {
      // x{{a,b}}y ==> x{a}y x{b}y
      n = expand$1(n[0], false).map(embrace);
      if (n.length === 1) {
        var post = m.post.length
          ? expand$1(m.post, false)
          : [''];
        return post.map(function(p) {
          return m.pre + n[0] + p;
        });
      }
    }
  }

  // at this point, n is the parts, and we know it's not a comma set
  // with a single entry.

  // no need to expand pre, since it is guaranteed to be free of brace-sets
  var pre = m.pre;
  var post = m.post.length
    ? expand$1(m.post, false)
    : [''];

  var N;

  if (isSequence) {
    var x = numeric(n[0]);
    var y = numeric(n[1]);
    var width = Math.max(n[0].length, n[1].length);
    var incr = n.length == 3
      ? Math.abs(numeric(n[2]))
      : 1;
    var test = lte;
    var reverse = y < x;
    if (reverse) {
      incr *= -1;
      test = gte;
    }
    var pad = n.some(isPadded);

    N = [];

    for (var i = x; test(i, y); i += incr) {
      var c;
      if (isAlphaSequence) {
        c = String.fromCharCode(i);
        if (c === '\\')
          c = '';
      } else {
        c = String(i);
        if (pad) {
          var need = width - c.length;
          if (need > 0) {
            var z = new Array(need + 1).join('0');
            if (i < 0)
              c = '-' + z + c.slice(1);
            else
              c = z + c;
          }
        }
      }
      N.push(c);
    }
  } else {
    N = concatMap(n, function(el) { return expand$1(el, false) });
  }

  for (var j = 0; j < N.length; j++) {
    for (var k = 0; k < post.length; k++) {
      var expansion = pre + N[j] + post[k];
      if (!isTop || isSequence || expansion)
        expansions.push(expansion);
    }
  }

  return expansions;
}

var minimatch_1 = minimatch$2;
minimatch$2.Minimatch = Minimatch$1;

var path$5 = (function () { try { return require('path') } catch (e) {}}()) || {
  sep: '/'
};
minimatch$2.sep = path$5.sep;

var GLOBSTAR = minimatch$2.GLOBSTAR = Minimatch$1.GLOBSTAR = {};
var expand = braceExpansion;

var plTypes = {
  '!': { open: '(?:(?!(?:', close: '))[^/]*?)'},
  '?': { open: '(?:', close: ')?' },
  '+': { open: '(?:', close: ')+' },
  '*': { open: '(?:', close: ')*' },
  '@': { open: '(?:', close: ')' }
};

// any single thing other than /
// don't need to escape / when using new RegExp()
var qmark = '[^/]';

// * => any number of characters
var star = qmark + '*?';

// ** when dots are allowed.  Anything goes, except .. and .
// not (^ or / followed by one or two dots followed by $ or /),
// followed by anything, any number of times.
var twoStarDot = '(?:(?!(?:\\\/|^)(?:\\.{1,2})($|\\\/)).)*?';

// not a ^ or / followed by a dot,
// followed by anything, any number of times.
var twoStarNoDot = '(?:(?!(?:\\\/|^)\\.).)*?';

// characters that need to be escaped in RegExp.
var reSpecials = charSet('().*{}+?[]^$\\!');

// "abc" -> { a:true, b:true, c:true }
function charSet (s) {
  return s.split('').reduce(function (set, c) {
    set[c] = true;
    return set
  }, {})
}

// normalizes slashes.
var slashSplit = /\/+/;

minimatch$2.filter = filter;
function filter (pattern, options) {
  options = options || {};
  return function (p, i, list) {
    return minimatch$2(p, pattern, options)
  }
}

function ext (a, b) {
  b = b || {};
  var t = {};
  Object.keys(a).forEach(function (k) {
    t[k] = a[k];
  });
  Object.keys(b).forEach(function (k) {
    t[k] = b[k];
  });
  return t
}

minimatch$2.defaults = function (def) {
  if (!def || typeof def !== 'object' || !Object.keys(def).length) {
    return minimatch$2
  }

  var orig = minimatch$2;

  var m = function minimatch (p, pattern, options) {
    return orig(p, pattern, ext(def, options))
  };

  m.Minimatch = function Minimatch (pattern, options) {
    return new orig.Minimatch(pattern, ext(def, options))
  };
  m.Minimatch.defaults = function defaults (options) {
    return orig.defaults(ext(def, options)).Minimatch
  };

  m.filter = function filter (pattern, options) {
    return orig.filter(pattern, ext(def, options))
  };

  m.defaults = function defaults (options) {
    return orig.defaults(ext(def, options))
  };

  m.makeRe = function makeRe (pattern, options) {
    return orig.makeRe(pattern, ext(def, options))
  };

  m.braceExpand = function braceExpand (pattern, options) {
    return orig.braceExpand(pattern, ext(def, options))
  };

  m.match = function (list, pattern, options) {
    return orig.match(list, pattern, ext(def, options))
  };

  return m
};

Minimatch$1.defaults = function (def) {
  return minimatch$2.defaults(def).Minimatch
};

function minimatch$2 (p, pattern, options) {
  assertValidPattern(pattern);

  if (!options) options = {};

  // shortcut: comments match nothing.
  if (!options.nocomment && pattern.charAt(0) === '#') {
    return false
  }

  return new Minimatch$1(pattern, options).match(p)
}

function Minimatch$1 (pattern, options) {
  if (!(this instanceof Minimatch$1)) {
    return new Minimatch$1(pattern, options)
  }

  assertValidPattern(pattern);

  if (!options) options = {};

  pattern = pattern.trim();

  // windows support: need to use /, not \
  if (!options.allowWindowsEscape && path$5.sep !== '/') {
    pattern = pattern.split(path$5.sep).join('/');
  }

  this.options = options;
  this.set = [];
  this.pattern = pattern;
  this.regexp = null;
  this.negate = false;
  this.comment = false;
  this.empty = false;
  this.partial = !!options.partial;

  // make the set of regexps etc.
  this.make();
}

Minimatch$1.prototype.debug = function () {};

Minimatch$1.prototype.make = make;
function make () {
  var pattern = this.pattern;
  var options = this.options;

  // empty patterns and comments match nothing.
  if (!options.nocomment && pattern.charAt(0) === '#') {
    this.comment = true;
    return
  }
  if (!pattern) {
    this.empty = true;
    return
  }

  // step 1: figure out negation, etc.
  this.parseNegate();

  // step 2: expand braces
  var set = this.globSet = this.braceExpand();

  if (options.debug) this.debug = function debug() { console.error.apply(console, arguments); };

  this.debug(this.pattern, set);

  // step 3: now we have a set, so turn each one into a series of path-portion
  // matching patterns.
  // These will be regexps, except in the case of "**", which is
  // set to the GLOBSTAR object for globstar behavior,
  // and will not contain any / characters
  set = this.globParts = set.map(function (s) {
    return s.split(slashSplit)
  });

  this.debug(this.pattern, set);

  // glob --> regexps
  set = set.map(function (s, si, set) {
    return s.map(this.parse, this)
  }, this);

  this.debug(this.pattern, set);

  // filter out everything that didn't compile properly.
  set = set.filter(function (s) {
    return s.indexOf(false) === -1
  });

  this.debug(this.pattern, set);

  this.set = set;
}

Minimatch$1.prototype.parseNegate = parseNegate;
function parseNegate () {
  var pattern = this.pattern;
  var negate = false;
  var options = this.options;
  var negateOffset = 0;

  if (options.nonegate) return

  for (var i = 0, l = pattern.length
    ; i < l && pattern.charAt(i) === '!'
    ; i++) {
    negate = !negate;
    negateOffset++;
  }

  if (negateOffset) this.pattern = pattern.substr(negateOffset);
  this.negate = negate;
}

// Brace expansion:
// a{b,c}d -> abd acd
// a{b,}c -> abc ac
// a{0..3}d -> a0d a1d a2d a3d
// a{b,c{d,e}f}g -> abg acdfg acefg
// a{b,c}d{e,f}g -> abdeg acdeg abdeg abdfg
//
// Invalid sets are not expanded.
// a{2..}b -> a{2..}b
// a{b}c -> a{b}c
minimatch$2.braceExpand = function (pattern, options) {
  return braceExpand(pattern, options)
};

Minimatch$1.prototype.braceExpand = braceExpand;

function braceExpand (pattern, options) {
  if (!options) {
    if (this instanceof Minimatch$1) {
      options = this.options;
    } else {
      options = {};
    }
  }

  pattern = typeof pattern === 'undefined'
    ? this.pattern : pattern;

  assertValidPattern(pattern);

  // Thanks to Yeting Li <https://github.com/yetingli> for
  // improving this regexp to avoid a ReDOS vulnerability.
  if (options.nobrace || !/\{(?:(?!\{).)*\}/.test(pattern)) {
    // shortcut. no need to expand.
    return [pattern]
  }

  return expand(pattern)
}

var MAX_PATTERN_LENGTH = 1024 * 64;
var assertValidPattern = function (pattern) {
  if (typeof pattern !== 'string') {
    throw new TypeError('invalid pattern')
  }

  if (pattern.length > MAX_PATTERN_LENGTH) {
    throw new TypeError('pattern is too long')
  }
};

// parse a component of the expanded set.
// At this point, no pattern may contain "/" in it
// so we're going to return a 2d array, where each entry is the full
// pattern, split on '/', and then turned into a regular expression.
// A regexp is made at the end which joins each array with an
// escaped /, and another full one which joins each regexp with |.
//
// Following the lead of Bash 4.1, note that "**" only has special meaning
// when it is the *only* thing in a path portion.  Otherwise, any series
// of * is equivalent to a single *.  Globstar behavior is enabled by
// default, and can be disabled by setting options.noglobstar.
Minimatch$1.prototype.parse = parse;
var SUBPARSE = {};
function parse (pattern, isSub) {
  assertValidPattern(pattern);

  var options = this.options;

  // shortcuts
  if (pattern === '**') {
    if (!options.noglobstar)
      return GLOBSTAR
    else
      pattern = '*';
  }
  if (pattern === '') return ''

  var re = '';
  var hasMagic = !!options.nocase;
  var escaping = false;
  // ? => one single character
  var patternListStack = [];
  var negativeLists = [];
  var stateChar;
  var inClass = false;
  var reClassStart = -1;
  var classStart = -1;
  // . and .. never match anything that doesn't start with .,
  // even when options.dot is set.
  var patternStart = pattern.charAt(0) === '.' ? '' // anything
  // not (start or / followed by . or .. followed by / or end)
  : options.dot ? '(?!(?:^|\\\/)\\.{1,2}(?:$|\\\/))'
  : '(?!\\.)';
  var self = this;

  function clearStateChar () {
    if (stateChar) {
      // we had some state-tracking character
      // that wasn't consumed by this pass.
      switch (stateChar) {
        case '*':
          re += star;
          hasMagic = true;
        break
        case '?':
          re += qmark;
          hasMagic = true;
        break
        default:
          re += '\\' + stateChar;
        break
      }
      self.debug('clearStateChar %j %j', stateChar, re);
      stateChar = false;
    }
  }

  for (var i = 0, len = pattern.length, c
    ; (i < len) && (c = pattern.charAt(i))
    ; i++) {
    this.debug('%s\t%s %s %j', pattern, i, re, c);

    // skip over any that are escaped.
    if (escaping && reSpecials[c]) {
      re += '\\' + c;
      escaping = false;
      continue
    }

    switch (c) {
      /* istanbul ignore next */
      case '/': {
        // completely not allowed, even escaped.
        // Should already be path-split by now.
        return false
      }

      case '\\':
        clearStateChar();
        escaping = true;
      continue

      // the various stateChar values
      // for the "extglob" stuff.
      case '?':
      case '*':
      case '+':
      case '@':
      case '!':
        this.debug('%s\t%s %s %j <-- stateChar', pattern, i, re, c);

        // all of those are literals inside a class, except that
        // the glob [!a] means [^a] in regexp
        if (inClass) {
          this.debug('  in class');
          if (c === '!' && i === classStart + 1) c = '^';
          re += c;
          continue
        }

        // if we already have a stateChar, then it means
        // that there was something like ** or +? in there.
        // Handle the stateChar, then proceed with this one.
        self.debug('call clearStateChar %j', stateChar);
        clearStateChar();
        stateChar = c;
        // if extglob is disabled, then +(asdf|foo) isn't a thing.
        // just clear the statechar *now*, rather than even diving into
        // the patternList stuff.
        if (options.noext) clearStateChar();
      continue

      case '(':
        if (inClass) {
          re += '(';
          continue
        }

        if (!stateChar) {
          re += '\\(';
          continue
        }

        patternListStack.push({
          type: stateChar,
          start: i - 1,
          reStart: re.length,
          open: plTypes[stateChar].open,
          close: plTypes[stateChar].close
        });
        // negation is (?:(?!js)[^/]*)
        re += stateChar === '!' ? '(?:(?!(?:' : '(?:';
        this.debug('plType %j %j', stateChar, re);
        stateChar = false;
      continue

      case ')':
        if (inClass || !patternListStack.length) {
          re += '\\)';
          continue
        }

        clearStateChar();
        hasMagic = true;
        var pl = patternListStack.pop();
        // negation is (?:(?!js)[^/]*)
        // The others are (?:<pattern>)<type>
        re += pl.close;
        if (pl.type === '!') {
          negativeLists.push(pl);
        }
        pl.reEnd = re.length;
      continue

      case '|':
        if (inClass || !patternListStack.length || escaping) {
          re += '\\|';
          escaping = false;
          continue
        }

        clearStateChar();
        re += '|';
      continue

      // these are mostly the same in regexp and glob
      case '[':
        // swallow any state-tracking char before the [
        clearStateChar();

        if (inClass) {
          re += '\\' + c;
          continue
        }

        inClass = true;
        classStart = i;
        reClassStart = re.length;
        re += c;
      continue

      case ']':
        //  a right bracket shall lose its special
        //  meaning and represent itself in
        //  a bracket expression if it occurs
        //  first in the list.  -- POSIX.2 2.8.3.2
        if (i === classStart + 1 || !inClass) {
          re += '\\' + c;
          escaping = false;
          continue
        }

        // handle the case where we left a class open.
        // "[z-a]" is valid, equivalent to "\[z-a\]"
        // split where the last [ was, make sure we don't have
        // an invalid re. if so, re-walk the contents of the
        // would-be class to re-translate any characters that
        // were passed through as-is
        // TODO: It would probably be faster to determine this
        // without a try/catch and a new RegExp, but it's tricky
        // to do safely.  For now, this is safe and works.
        var cs = pattern.substring(classStart + 1, i);
        try {
          RegExp('[' + cs + ']');
        } catch (er) {
          // not a valid class!
          var sp = this.parse(cs, SUBPARSE);
          re = re.substr(0, reClassStart) + '\\[' + sp[0] + '\\]';
          hasMagic = hasMagic || sp[1];
          inClass = false;
          continue
        }

        // finish up the class.
        hasMagic = true;
        inClass = false;
        re += c;
      continue

      default:
        // swallow any state char that wasn't consumed
        clearStateChar();

        if (escaping) {
          // no need
          escaping = false;
        } else if (reSpecials[c]
          && !(c === '^' && inClass)) {
          re += '\\';
        }

        re += c;

    } // switch
  } // for

  // handle the case where we left a class open.
  // "[abc" is valid, equivalent to "\[abc"
  if (inClass) {
    // split where the last [ was, and escape it
    // this is a huge pita.  We now have to re-walk
    // the contents of the would-be class to re-translate
    // any characters that were passed through as-is
    cs = pattern.substr(classStart + 1);
    sp = this.parse(cs, SUBPARSE);
    re = re.substr(0, reClassStart) + '\\[' + sp[0];
    hasMagic = hasMagic || sp[1];
  }

  // handle the case where we had a +( thing at the *end*
  // of the pattern.
  // each pattern list stack adds 3 chars, and we need to go through
  // and escape any | chars that were passed through as-is for the regexp.
  // Go through and escape them, taking care not to double-escape any
  // | chars that were already escaped.
  for (pl = patternListStack.pop(); pl; pl = patternListStack.pop()) {
    var tail = re.slice(pl.reStart + pl.open.length);
    this.debug('setting tail', re, pl);
    // maybe some even number of \, then maybe 1 \, followed by a |
    tail = tail.replace(/((?:\\{2}){0,64})(\\?)\|/g, function (_, $1, $2) {
      if (!$2) {
        // the | isn't already escaped, so escape it.
        $2 = '\\';
      }

      // need to escape all those slashes *again*, without escaping the
      // one that we need for escaping the | character.  As it works out,
      // escaping an even number of slashes can be done by simply repeating
      // it exactly after itself.  That's why this trick works.
      //
      // I am sorry that you have to see this.
      return $1 + $1 + $2 + '|'
    });

    this.debug('tail=%j\n   %s', tail, tail, pl, re);
    var t = pl.type === '*' ? star
      : pl.type === '?' ? qmark
      : '\\' + pl.type;

    hasMagic = true;
    re = re.slice(0, pl.reStart) + t + '\\(' + tail;
  }

  // handle trailing things that only matter at the very end.
  clearStateChar();
  if (escaping) {
    // trailing \\
    re += '\\\\';
  }

  // only need to apply the nodot start if the re starts with
  // something that could conceivably capture a dot
  var addPatternStart = false;
  switch (re.charAt(0)) {
    case '[': case '.': case '(': addPatternStart = true;
  }

  // Hack to work around lack of negative lookbehind in JS
  // A pattern like: *.!(x).!(y|z) needs to ensure that a name
  // like 'a.xyz.yz' doesn't match.  So, the first negative
  // lookahead, has to look ALL the way ahead, to the end of
  // the pattern.
  for (var n = negativeLists.length - 1; n > -1; n--) {
    var nl = negativeLists[n];

    var nlBefore = re.slice(0, nl.reStart);
    var nlFirst = re.slice(nl.reStart, nl.reEnd - 8);
    var nlLast = re.slice(nl.reEnd - 8, nl.reEnd);
    var nlAfter = re.slice(nl.reEnd);

    nlLast += nlAfter;

    // Handle nested stuff like *(*.js|!(*.json)), where open parens
    // mean that we should *not* include the ) in the bit that is considered
    // "after" the negated section.
    var openParensBefore = nlBefore.split('(').length - 1;
    var cleanAfter = nlAfter;
    for (i = 0; i < openParensBefore; i++) {
      cleanAfter = cleanAfter.replace(/\)[+*?]?/, '');
    }
    nlAfter = cleanAfter;

    var dollar = '';
    if (nlAfter === '' && isSub !== SUBPARSE) {
      dollar = '$';
    }
    var newRe = nlBefore + nlFirst + nlAfter + dollar + nlLast;
    re = newRe;
  }

  // if the re is not "" at this point, then we need to make sure
  // it doesn't match against an empty path part.
  // Otherwise a/* will match a/, which it should not.
  if (re !== '' && hasMagic) {
    re = '(?=.)' + re;
  }

  if (addPatternStart) {
    re = patternStart + re;
  }

  // parsing just a piece of a larger pattern.
  if (isSub === SUBPARSE) {
    return [re, hasMagic]
  }

  // skip the regexp for non-magical patterns
  // unescape anything in it, though, so that it'll be
  // an exact match against a file etc.
  if (!hasMagic) {
    return globUnescape(pattern)
  }

  var flags = options.nocase ? 'i' : '';
  try {
    var regExp = new RegExp('^' + re + '$', flags);
  } catch (er) /* istanbul ignore next - should be impossible */ {
    // If it was an invalid regular expression, then it can't match
    // anything.  This trick looks for a character after the end of
    // the string, which is of course impossible, except in multi-line
    // mode, but it's not a /m regex.
    return new RegExp('$.')
  }

  regExp._glob = pattern;
  regExp._src = re;

  return regExp
}

minimatch$2.makeRe = function (pattern, options) {
  return new Minimatch$1(pattern, options || {}).makeRe()
};

Minimatch$1.prototype.makeRe = makeRe;
function makeRe () {
  if (this.regexp || this.regexp === false) return this.regexp

  // at this point, this.set is a 2d array of partial
  // pattern strings, or "**".
  //
  // It's better to use .match().  This function shouldn't
  // be used, really, but it's pretty convenient sometimes,
  // when you just want to work with a regex.
  var set = this.set;

  if (!set.length) {
    this.regexp = false;
    return this.regexp
  }
  var options = this.options;

  var twoStar = options.noglobstar ? star
    : options.dot ? twoStarDot
    : twoStarNoDot;
  var flags = options.nocase ? 'i' : '';

  var re = set.map(function (pattern) {
    return pattern.map(function (p) {
      return (p === GLOBSTAR) ? twoStar
      : (typeof p === 'string') ? regExpEscape(p)
      : p._src
    }).join('\\\/')
  }).join('|');

  // must match entire pattern
  // ending in a * or ** will make it less strict.
  re = '^(?:' + re + ')$';

  // can match anything, as long as it's not this.
  if (this.negate) re = '^(?!' + re + ').*$';

  try {
    this.regexp = new RegExp(re, flags);
  } catch (ex) /* istanbul ignore next - should be impossible */ {
    this.regexp = false;
  }
  return this.regexp
}

minimatch$2.match = function (list, pattern, options) {
  options = options || {};
  var mm = new Minimatch$1(pattern, options);
  list = list.filter(function (f) {
    return mm.match(f)
  });
  if (mm.options.nonull && !list.length) {
    list.push(pattern);
  }
  return list
};

Minimatch$1.prototype.match = function match (f, partial) {
  if (typeof partial === 'undefined') partial = this.partial;
  this.debug('match', f, this.pattern);
  // short-circuit in the case of busted things.
  // comments, etc.
  if (this.comment) return false
  if (this.empty) return f === ''

  if (f === '/' && partial) return true

  var options = this.options;

  // windows: need to use /, not \
  if (path$5.sep !== '/') {
    f = f.split(path$5.sep).join('/');
  }

  // treat the test path as a set of pathparts.
  f = f.split(slashSplit);
  this.debug(this.pattern, 'split', f);

  // just ONE of the pattern sets in this.set needs to match
  // in order for it to be valid.  If negating, then just one
  // match means that we have failed.
  // Either way, return on the first hit.

  var set = this.set;
  this.debug(this.pattern, 'set', set);

  // Find the basename of the path by looking for the last non-empty segment
  var filename;
  var i;
  for (i = f.length - 1; i >= 0; i--) {
    filename = f[i];
    if (filename) break
  }

  for (i = 0; i < set.length; i++) {
    var pattern = set[i];
    var file = f;
    if (options.matchBase && pattern.length === 1) {
      file = [filename];
    }
    var hit = this.matchOne(file, pattern, partial);
    if (hit) {
      if (options.flipNegate) return true
      return !this.negate
    }
  }

  // didn't get any hits.  this is success if it's a negative
  // pattern, failure otherwise.
  if (options.flipNegate) return false
  return this.negate
};

// set partial to true to test if, for example,
// "/a/b" matches the start of "/*/b/*/d"
// Partial means, if you run out of file before you run
// out of pattern, then that's fine, as long as all
// the parts match.
Minimatch$1.prototype.matchOne = function (file, pattern, partial) {
  var options = this.options;

  this.debug('matchOne',
    { 'this': this, file: file, pattern: pattern });

  this.debug('matchOne', file.length, pattern.length);

  for (var fi = 0,
      pi = 0,
      fl = file.length,
      pl = pattern.length
      ; (fi < fl) && (pi < pl)
      ; fi++, pi++) {
    this.debug('matchOne loop');
    var p = pattern[pi];
    var f = file[fi];

    this.debug(pattern, p, f);

    // should be impossible.
    // some invalid regexp stuff in the set.
    /* istanbul ignore if */
    if (p === false) return false

    if (p === GLOBSTAR) {
      this.debug('GLOBSTAR', [pattern, p, f]);

      // "**"
      // a/**/b/**/c would match the following:
      // a/b/x/y/z/c
      // a/x/y/z/b/c
      // a/b/x/b/x/c
      // a/b/c
      // To do this, take the rest of the pattern after
      // the **, and see if it would match the file remainder.
      // If so, return success.
      // If not, the ** "swallows" a segment, and try again.
      // This is recursively awful.
      //
      // a/**/b/**/c matching a/b/x/y/z/c
      // - a matches a
      // - doublestar
      //   - matchOne(b/x/y/z/c, b/**/c)
      //     - b matches b
      //     - doublestar
      //       - matchOne(x/y/z/c, c) -> no
      //       - matchOne(y/z/c, c) -> no
      //       - matchOne(z/c, c) -> no
      //       - matchOne(c, c) yes, hit
      var fr = fi;
      var pr = pi + 1;
      if (pr === pl) {
        this.debug('** at the end');
        // a ** at the end will just swallow the rest.
        // We have found a match.
        // however, it will not swallow /.x, unless
        // options.dot is set.
        // . and .. are *never* matched by **, for explosively
        // exponential reasons.
        for (; fi < fl; fi++) {
          if (file[fi] === '.' || file[fi] === '..' ||
            (!options.dot && file[fi].charAt(0) === '.')) return false
        }
        return true
      }

      // ok, let's see if we can swallow whatever we can.
      while (fr < fl) {
        var swallowee = file[fr];

        this.debug('\nglobstar while', file, fr, pattern, pr, swallowee);

        // XXX remove this slice.  Just pass the start index.
        if (this.matchOne(file.slice(fr), pattern.slice(pr), partial)) {
          this.debug('globstar found match!', fr, fl, swallowee);
          // found a match.
          return true
        } else {
          // can't swallow "." or ".." ever.
          // can only swallow ".foo" when explicitly asked.
          if (swallowee === '.' || swallowee === '..' ||
            (!options.dot && swallowee.charAt(0) === '.')) {
            this.debug('dot detected!', file, fr, pattern, pr);
            break
          }

          // ** swallows a segment, and continue.
          this.debug('globstar swallow a segment, and continue');
          fr++;
        }
      }

      // no match was found.
      // However, in partial mode, we can't say this is necessarily over.
      // If there's more *pattern* left, then
      /* istanbul ignore if */
      if (partial) {
        // ran out of file
        this.debug('\n>>> no match, partial?', file, fr, pattern, pr);
        if (fr === fl) return true
      }
      return false
    }

    // something other than **
    // non-magic patterns just have to match exactly
    // patterns with magic have been turned into regexps.
    var hit;
    if (typeof p === 'string') {
      hit = f === p;
      this.debug('string match', p, f, hit);
    } else {
      hit = f.match(p);
      this.debug('pattern match', p, f, hit);
    }

    if (!hit) return false
  }

  // Note: ending in / means that we'll get a final ""
  // at the end of the pattern.  This can only match a
  // corresponding "" at the end of the file.
  // If the file ends in /, then it can only match a
  // a pattern that ends in /, unless the pattern just
  // doesn't have any more for it. But, a/b/ should *not*
  // match "a/b/*", even though "" matches against the
  // [^/]*? pattern, except in partial mode, where it might
  // simply not be reached yet.
  // However, a/b/ should still satisfy a/*

  // now either we fell off the end of the pattern, or we're done.
  if (fi === fl && pi === pl) {
    // ran out of pattern and filename at the same time.
    // an exact hit!
    return true
  } else if (fi === fl) {
    // ran out of file, but still had pattern left.
    // this is ok if we're doing the match as part of
    // a glob fs traversal.
    return partial
  } else /* istanbul ignore else */ if (pi === pl) {
    // ran out of pattern, still have file left.
    // this is only acceptable if we're on the very last
    // empty segment of a file with a trailing slash.
    // a/* should match a/b/
    return (fi === fl - 1) && (file[fi] === '')
  }

  // should be unreachable.
  /* istanbul ignore next */
  throw new Error('wtf?')
};

// replace stuff like \* with *
function globUnescape (s) {
  return s.replace(/\\(.)/g, '$1')
}

function regExpEscape (s) {
  return s.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, '\\$&')
}

var inherits = {exports: {}};

var inherits_browser = {exports: {}};

var hasRequiredInherits_browser;

function requireInherits_browser () {
	if (hasRequiredInherits_browser) return inherits_browser.exports;
	hasRequiredInherits_browser = 1;
	if (typeof Object.create === 'function') {
	  // implementation from standard node.js 'util' module
	  inherits_browser.exports = function inherits(ctor, superCtor) {
	    if (superCtor) {
	      ctor.super_ = superCtor;
	      ctor.prototype = Object.create(superCtor.prototype, {
	        constructor: {
	          value: ctor,
	          enumerable: false,
	          writable: true,
	          configurable: true
	        }
	      });
	    }
	  };
	} else {
	  // old school shim for old browsers
	  inherits_browser.exports = function inherits(ctor, superCtor) {
	    if (superCtor) {
	      ctor.super_ = superCtor;
	      var TempCtor = function () {};
	      TempCtor.prototype = superCtor.prototype;
	      ctor.prototype = new TempCtor();
	      ctor.prototype.constructor = ctor;
	    }
	  };
	}
	return inherits_browser.exports;
}

(function (module) {
	try {
	  var util = require('util');
	  /* istanbul ignore next */
	  if (typeof util.inherits !== 'function') throw '';
	  module.exports = util.inherits;
	} catch (e) {
	  /* istanbul ignore next */
	  module.exports = requireInherits_browser();
	}
} (inherits));

var pathIsAbsolute = {exports: {}};

function posix(path) {
	return path.charAt(0) === '/';
}

function win32(path) {
	// https://github.com/nodejs/node/blob/b3fcc245fb25539909ef1d5eaa01dbf92e168633/lib/path.js#L56
	var splitDeviceRe = /^([a-zA-Z]:|[\\\/]{2}[^\\\/]+[\\\/]+[^\\\/]+)?([\\\/])?([\s\S]*?)$/;
	var result = splitDeviceRe.exec(path);
	var device = result[1] || '';
	var isUnc = Boolean(device && device.charAt(1) !== ':');

	// UNC paths are always absolute
	return Boolean(result[2] || isUnc);
}

pathIsAbsolute.exports = process.platform === 'win32' ? win32 : posix;
pathIsAbsolute.exports.posix = posix;
pathIsAbsolute.exports.win32 = win32;

var common = {};

common.setopts = setopts;
common.ownProp = ownProp;
common.makeAbs = makeAbs;
common.finish = finish;
common.mark = mark;
common.isIgnored = isIgnored;
common.childrenIgnored = childrenIgnored;

function ownProp (obj, field) {
  return Object.prototype.hasOwnProperty.call(obj, field)
}

var fs$3 = require$$0$1;
var path$4 = require$$0;
var minimatch$1 = minimatch_1;
var isAbsolute$2 = pathIsAbsolute.exports;
var Minimatch = minimatch$1.Minimatch;

function alphasort (a, b) {
  return a.localeCompare(b, 'en')
}

function setupIgnores (self, options) {
  self.ignore = options.ignore || [];

  if (!Array.isArray(self.ignore))
    self.ignore = [self.ignore];

  if (self.ignore.length) {
    self.ignore = self.ignore.map(ignoreMap);
  }
}

// ignore patterns are always in dot:true mode.
function ignoreMap (pattern) {
  var gmatcher = null;
  if (pattern.slice(-3) === '/**') {
    var gpattern = pattern.replace(/(\/\*\*)+$/, '');
    gmatcher = new Minimatch(gpattern, { dot: true });
  }

  return {
    matcher: new Minimatch(pattern, { dot: true }),
    gmatcher: gmatcher
  }
}

function setopts (self, pattern, options) {
  if (!options)
    options = {};

  // base-matching: just use globstar for that.
  if (options.matchBase && -1 === pattern.indexOf("/")) {
    if (options.noglobstar) {
      throw new Error("base matching requires globstar")
    }
    pattern = "**/" + pattern;
  }

  self.silent = !!options.silent;
  self.pattern = pattern;
  self.strict = options.strict !== false;
  self.realpath = !!options.realpath;
  self.realpathCache = options.realpathCache || Object.create(null);
  self.follow = !!options.follow;
  self.dot = !!options.dot;
  self.mark = !!options.mark;
  self.nodir = !!options.nodir;
  if (self.nodir)
    self.mark = true;
  self.sync = !!options.sync;
  self.nounique = !!options.nounique;
  self.nonull = !!options.nonull;
  self.nosort = !!options.nosort;
  self.nocase = !!options.nocase;
  self.stat = !!options.stat;
  self.noprocess = !!options.noprocess;
  self.absolute = !!options.absolute;
  self.fs = options.fs || fs$3;

  self.maxLength = options.maxLength || Infinity;
  self.cache = options.cache || Object.create(null);
  self.statCache = options.statCache || Object.create(null);
  self.symlinks = options.symlinks || Object.create(null);

  setupIgnores(self, options);

  self.changedCwd = false;
  var cwd = process.cwd();
  if (!ownProp(options, "cwd"))
    self.cwd = cwd;
  else {
    self.cwd = path$4.resolve(options.cwd);
    self.changedCwd = self.cwd !== cwd;
  }

  self.root = options.root || path$4.resolve(self.cwd, "/");
  self.root = path$4.resolve(self.root);
  if (process.platform === "win32")
    self.root = self.root.replace(/\\/g, "/");

  // TODO: is an absolute `cwd` supposed to be resolved against `root`?
  // e.g. { cwd: '/test', root: __dirname } === path.join(__dirname, '/test')
  self.cwdAbs = isAbsolute$2(self.cwd) ? self.cwd : makeAbs(self, self.cwd);
  if (process.platform === "win32")
    self.cwdAbs = self.cwdAbs.replace(/\\/g, "/");
  self.nomount = !!options.nomount;

  // disable comments and negation in Minimatch.
  // Note that they are not supported in Glob itself anyway.
  options.nonegate = true;
  options.nocomment = true;
  // always treat \ in patterns as escapes, not path separators
  options.allowWindowsEscape = false;

  self.minimatch = new Minimatch(pattern, options);
  self.options = self.minimatch.options;
}

function finish (self) {
  var nou = self.nounique;
  var all = nou ? [] : Object.create(null);

  for (var i = 0, l = self.matches.length; i < l; i ++) {
    var matches = self.matches[i];
    if (!matches || Object.keys(matches).length === 0) {
      if (self.nonull) {
        // do like the shell, and spit out the literal glob
        var literal = self.minimatch.globSet[i];
        if (nou)
          all.push(literal);
        else
          all[literal] = true;
      }
    } else {
      // had matches
      var m = Object.keys(matches);
      if (nou)
        all.push.apply(all, m);
      else
        m.forEach(function (m) {
          all[m] = true;
        });
    }
  }

  if (!nou)
    all = Object.keys(all);

  if (!self.nosort)
    all = all.sort(alphasort);

  // at *some* point we statted all of these
  if (self.mark) {
    for (var i = 0; i < all.length; i++) {
      all[i] = self._mark(all[i]);
    }
    if (self.nodir) {
      all = all.filter(function (e) {
        var notDir = !(/\/$/.test(e));
        var c = self.cache[e] || self.cache[makeAbs(self, e)];
        if (notDir && c)
          notDir = c !== 'DIR' && !Array.isArray(c);
        return notDir
      });
    }
  }

  if (self.ignore.length)
    all = all.filter(function(m) {
      return !isIgnored(self, m)
    });

  self.found = all;
}

function mark (self, p) {
  var abs = makeAbs(self, p);
  var c = self.cache[abs];
  var m = p;
  if (c) {
    var isDir = c === 'DIR' || Array.isArray(c);
    var slash = p.slice(-1) === '/';

    if (isDir && !slash)
      m += '/';
    else if (!isDir && slash)
      m = m.slice(0, -1);

    if (m !== p) {
      var mabs = makeAbs(self, m);
      self.statCache[mabs] = self.statCache[abs];
      self.cache[mabs] = self.cache[abs];
    }
  }

  return m
}

// lotta situps...
function makeAbs (self, f) {
  var abs = f;
  if (f.charAt(0) === '/') {
    abs = path$4.join(self.root, f);
  } else if (isAbsolute$2(f) || f === '') {
    abs = f;
  } else if (self.changedCwd) {
    abs = path$4.resolve(self.cwd, f);
  } else {
    abs = path$4.resolve(f);
  }

  if (process.platform === 'win32')
    abs = abs.replace(/\\/g, '/');

  return abs
}


// Return true, if pattern ends with globstar '**', for the accompanying parent directory.
// Ex:- If node_modules/** is the pattern, add 'node_modules' to ignore list along with it's contents
function isIgnored (self, path) {
  if (!self.ignore.length)
    return false

  return self.ignore.some(function(item) {
    return item.matcher.match(path) || !!(item.gmatcher && item.gmatcher.match(path))
  })
}

function childrenIgnored (self, path) {
  if (!self.ignore.length)
    return false

  return self.ignore.some(function(item) {
    return !!(item.gmatcher && item.gmatcher.match(path))
  })
}

var sync;
var hasRequiredSync;

function requireSync () {
	if (hasRequiredSync) return sync;
	hasRequiredSync = 1;
	sync = globSync;
	globSync.GlobSync = GlobSync;

	var rp = fs_realpath;
	var minimatch = minimatch_1;
	minimatch.Minimatch;
	requireGlob().Glob;
	var path = require$$0;
	var assert = require$$5;
	var isAbsolute = pathIsAbsolute.exports;
	var common$1 = common;
	var setopts = common$1.setopts;
	var ownProp = common$1.ownProp;
	var childrenIgnored = common$1.childrenIgnored;
	var isIgnored = common$1.isIgnored;

	function globSync (pattern, options) {
	  if (typeof options === 'function' || arguments.length === 3)
	    throw new TypeError('callback provided to sync glob\n'+
	                        'See: https://github.com/isaacs/node-glob/issues/167')

	  return new GlobSync(pattern, options).found
	}

	function GlobSync (pattern, options) {
	  if (!pattern)
	    throw new Error('must provide pattern')

	  if (typeof options === 'function' || arguments.length === 3)
	    throw new TypeError('callback provided to sync glob\n'+
	                        'See: https://github.com/isaacs/node-glob/issues/167')

	  if (!(this instanceof GlobSync))
	    return new GlobSync(pattern, options)

	  setopts(this, pattern, options);

	  if (this.noprocess)
	    return this

	  var n = this.minimatch.set.length;
	  this.matches = new Array(n);
	  for (var i = 0; i < n; i ++) {
	    this._process(this.minimatch.set[i], i, false);
	  }
	  this._finish();
	}

	GlobSync.prototype._finish = function () {
	  assert.ok(this instanceof GlobSync);
	  if (this.realpath) {
	    var self = this;
	    this.matches.forEach(function (matchset, index) {
	      var set = self.matches[index] = Object.create(null);
	      for (var p in matchset) {
	        try {
	          p = self._makeAbs(p);
	          var real = rp.realpathSync(p, self.realpathCache);
	          set[real] = true;
	        } catch (er) {
	          if (er.syscall === 'stat')
	            set[self._makeAbs(p)] = true;
	          else
	            throw er
	        }
	      }
	    });
	  }
	  common$1.finish(this);
	};


	GlobSync.prototype._process = function (pattern, index, inGlobStar) {
	  assert.ok(this instanceof GlobSync);

	  // Get the first [n] parts of pattern that are all strings.
	  var n = 0;
	  while (typeof pattern[n] === 'string') {
	    n ++;
	  }
	  // now n is the index of the first one that is *not* a string.

	  // See if there's anything else
	  var prefix;
	  switch (n) {
	    // if not, then this is rather simple
	    case pattern.length:
	      this._processSimple(pattern.join('/'), index);
	      return

	    case 0:
	      // pattern *starts* with some non-trivial item.
	      // going to readdir(cwd), but not include the prefix in matches.
	      prefix = null;
	      break

	    default:
	      // pattern has some string bits in the front.
	      // whatever it starts with, whether that's 'absolute' like /foo/bar,
	      // or 'relative' like '../baz'
	      prefix = pattern.slice(0, n).join('/');
	      break
	  }

	  var remain = pattern.slice(n);

	  // get the list of entries.
	  var read;
	  if (prefix === null)
	    read = '.';
	  else if (isAbsolute(prefix) ||
	      isAbsolute(pattern.map(function (p) {
	        return typeof p === 'string' ? p : '[*]'
	      }).join('/'))) {
	    if (!prefix || !isAbsolute(prefix))
	      prefix = '/' + prefix;
	    read = prefix;
	  } else
	    read = prefix;

	  var abs = this._makeAbs(read);

	  //if ignored, skip processing
	  if (childrenIgnored(this, read))
	    return

	  var isGlobStar = remain[0] === minimatch.GLOBSTAR;
	  if (isGlobStar)
	    this._processGlobStar(prefix, read, abs, remain, index, inGlobStar);
	  else
	    this._processReaddir(prefix, read, abs, remain, index, inGlobStar);
	};


	GlobSync.prototype._processReaddir = function (prefix, read, abs, remain, index, inGlobStar) {
	  var entries = this._readdir(abs, inGlobStar);

	  // if the abs isn't a dir, then nothing can match!
	  if (!entries)
	    return

	  // It will only match dot entries if it starts with a dot, or if
	  // dot is set.  Stuff like @(.foo|.bar) isn't allowed.
	  var pn = remain[0];
	  var negate = !!this.minimatch.negate;
	  var rawGlob = pn._glob;
	  var dotOk = this.dot || rawGlob.charAt(0) === '.';

	  var matchedEntries = [];
	  for (var i = 0; i < entries.length; i++) {
	    var e = entries[i];
	    if (e.charAt(0) !== '.' || dotOk) {
	      var m;
	      if (negate && !prefix) {
	        m = !e.match(pn);
	      } else {
	        m = e.match(pn);
	      }
	      if (m)
	        matchedEntries.push(e);
	    }
	  }

	  var len = matchedEntries.length;
	  // If there are no matched entries, then nothing matches.
	  if (len === 0)
	    return

	  // if this is the last remaining pattern bit, then no need for
	  // an additional stat *unless* the user has specified mark or
	  // stat explicitly.  We know they exist, since readdir returned
	  // them.

	  if (remain.length === 1 && !this.mark && !this.stat) {
	    if (!this.matches[index])
	      this.matches[index] = Object.create(null);

	    for (var i = 0; i < len; i ++) {
	      var e = matchedEntries[i];
	      if (prefix) {
	        if (prefix.slice(-1) !== '/')
	          e = prefix + '/' + e;
	        else
	          e = prefix + e;
	      }

	      if (e.charAt(0) === '/' && !this.nomount) {
	        e = path.join(this.root, e);
	      }
	      this._emitMatch(index, e);
	    }
	    // This was the last one, and no stats were needed
	    return
	  }

	  // now test all matched entries as stand-ins for that part
	  // of the pattern.
	  remain.shift();
	  for (var i = 0; i < len; i ++) {
	    var e = matchedEntries[i];
	    var newPattern;
	    if (prefix)
	      newPattern = [prefix, e];
	    else
	      newPattern = [e];
	    this._process(newPattern.concat(remain), index, inGlobStar);
	  }
	};


	GlobSync.prototype._emitMatch = function (index, e) {
	  if (isIgnored(this, e))
	    return

	  var abs = this._makeAbs(e);

	  if (this.mark)
	    e = this._mark(e);

	  if (this.absolute) {
	    e = abs;
	  }

	  if (this.matches[index][e])
	    return

	  if (this.nodir) {
	    var c = this.cache[abs];
	    if (c === 'DIR' || Array.isArray(c))
	      return
	  }

	  this.matches[index][e] = true;

	  if (this.stat)
	    this._stat(e);
	};


	GlobSync.prototype._readdirInGlobStar = function (abs) {
	  // follow all symlinked directories forever
	  // just proceed as if this is a non-globstar situation
	  if (this.follow)
	    return this._readdir(abs, false)

	  var entries;
	  var lstat;
	  try {
	    lstat = this.fs.lstatSync(abs);
	  } catch (er) {
	    if (er.code === 'ENOENT') {
	      // lstat failed, doesn't exist
	      return null
	    }
	  }

	  var isSym = lstat && lstat.isSymbolicLink();
	  this.symlinks[abs] = isSym;

	  // If it's not a symlink or a dir, then it's definitely a regular file.
	  // don't bother doing a readdir in that case.
	  if (!isSym && lstat && !lstat.isDirectory())
	    this.cache[abs] = 'FILE';
	  else
	    entries = this._readdir(abs, false);

	  return entries
	};

	GlobSync.prototype._readdir = function (abs, inGlobStar) {

	  if (inGlobStar && !ownProp(this.symlinks, abs))
	    return this._readdirInGlobStar(abs)

	  if (ownProp(this.cache, abs)) {
	    var c = this.cache[abs];
	    if (!c || c === 'FILE')
	      return null

	    if (Array.isArray(c))
	      return c
	  }

	  try {
	    return this._readdirEntries(abs, this.fs.readdirSync(abs))
	  } catch (er) {
	    this._readdirError(abs, er);
	    return null
	  }
	};

	GlobSync.prototype._readdirEntries = function (abs, entries) {
	  // if we haven't asked to stat everything, then just
	  // assume that everything in there exists, so we can avoid
	  // having to stat it a second time.
	  if (!this.mark && !this.stat) {
	    for (var i = 0; i < entries.length; i ++) {
	      var e = entries[i];
	      if (abs === '/')
	        e = abs + e;
	      else
	        e = abs + '/' + e;
	      this.cache[e] = true;
	    }
	  }

	  this.cache[abs] = entries;

	  // mark and cache dir-ness
	  return entries
	};

	GlobSync.prototype._readdirError = function (f, er) {
	  // handle errors, and cache the information
	  switch (er.code) {
	    case 'ENOTSUP': // https://github.com/isaacs/node-glob/issues/205
	    case 'ENOTDIR': // totally normal. means it *does* exist.
	      var abs = this._makeAbs(f);
	      this.cache[abs] = 'FILE';
	      if (abs === this.cwdAbs) {
	        var error = new Error(er.code + ' invalid cwd ' + this.cwd);
	        error.path = this.cwd;
	        error.code = er.code;
	        throw error
	      }
	      break

	    case 'ENOENT': // not terribly unusual
	    case 'ELOOP':
	    case 'ENAMETOOLONG':
	    case 'UNKNOWN':
	      this.cache[this._makeAbs(f)] = false;
	      break

	    default: // some unusual error.  Treat as failure.
	      this.cache[this._makeAbs(f)] = false;
	      if (this.strict)
	        throw er
	      if (!this.silent)
	        console.error('glob error', er);
	      break
	  }
	};

	GlobSync.prototype._processGlobStar = function (prefix, read, abs, remain, index, inGlobStar) {

	  var entries = this._readdir(abs, inGlobStar);

	  // no entries means not a dir, so it can never have matches
	  // foo.txt/** doesn't match foo.txt
	  if (!entries)
	    return

	  // test without the globstar, and with every child both below
	  // and replacing the globstar.
	  var remainWithoutGlobStar = remain.slice(1);
	  var gspref = prefix ? [ prefix ] : [];
	  var noGlobStar = gspref.concat(remainWithoutGlobStar);

	  // the noGlobStar pattern exits the inGlobStar state
	  this._process(noGlobStar, index, false);

	  var len = entries.length;
	  var isSym = this.symlinks[abs];

	  // If it's a symlink, and we're in a globstar, then stop
	  if (isSym && inGlobStar)
	    return

	  for (var i = 0; i < len; i++) {
	    var e = entries[i];
	    if (e.charAt(0) === '.' && !this.dot)
	      continue

	    // these two cases enter the inGlobStar state
	    var instead = gspref.concat(entries[i], remainWithoutGlobStar);
	    this._process(instead, index, true);

	    var below = gspref.concat(entries[i], remain);
	    this._process(below, index, true);
	  }
	};

	GlobSync.prototype._processSimple = function (prefix, index) {
	  // XXX review this.  Shouldn't it be doing the mounting etc
	  // before doing stat?  kinda weird?
	  var exists = this._stat(prefix);

	  if (!this.matches[index])
	    this.matches[index] = Object.create(null);

	  // If it doesn't exist, then just mark the lack of results
	  if (!exists)
	    return

	  if (prefix && isAbsolute(prefix) && !this.nomount) {
	    var trail = /[\/\\]$/.test(prefix);
	    if (prefix.charAt(0) === '/') {
	      prefix = path.join(this.root, prefix);
	    } else {
	      prefix = path.resolve(this.root, prefix);
	      if (trail)
	        prefix += '/';
	    }
	  }

	  if (process.platform === 'win32')
	    prefix = prefix.replace(/\\/g, '/');

	  // Mark this as a match
	  this._emitMatch(index, prefix);
	};

	// Returns either 'DIR', 'FILE', or false
	GlobSync.prototype._stat = function (f) {
	  var abs = this._makeAbs(f);
	  var needDir = f.slice(-1) === '/';

	  if (f.length > this.maxLength)
	    return false

	  if (!this.stat && ownProp(this.cache, abs)) {
	    var c = this.cache[abs];

	    if (Array.isArray(c))
	      c = 'DIR';

	    // It exists, but maybe not how we need it
	    if (!needDir || c === 'DIR')
	      return c

	    if (needDir && c === 'FILE')
	      return false

	    // otherwise we have to stat, because maybe c=true
	    // if we know it exists, but not what it is.
	  }
	  var stat = this.statCache[abs];
	  if (!stat) {
	    var lstat;
	    try {
	      lstat = this.fs.lstatSync(abs);
	    } catch (er) {
	      if (er && (er.code === 'ENOENT' || er.code === 'ENOTDIR')) {
	        this.statCache[abs] = false;
	        return false
	      }
	    }

	    if (lstat && lstat.isSymbolicLink()) {
	      try {
	        stat = this.fs.statSync(abs);
	      } catch (er) {
	        stat = lstat;
	      }
	    } else {
	      stat = lstat;
	    }
	  }

	  this.statCache[abs] = stat;

	  var c = true;
	  if (stat)
	    c = stat.isDirectory() ? 'DIR' : 'FILE';

	  this.cache[abs] = this.cache[abs] || c;

	  if (needDir && c === 'FILE')
	    return false

	  return c
	};

	GlobSync.prototype._mark = function (p) {
	  return common$1.mark(this, p)
	};

	GlobSync.prototype._makeAbs = function (f) {
	  return common$1.makeAbs(this, f)
	};
	return sync;
}

// Returns a wrapper function that returns a wrapped callback
// The wrapper function should do some stuff, and return a
// presumably different callback function.
// This makes sure that own properties are retained, so that
// decorations and such are not lost along the way.
var wrappy_1 = wrappy$2;
function wrappy$2 (fn, cb) {
  if (fn && cb) return wrappy$2(fn)(cb)

  if (typeof fn !== 'function')
    throw new TypeError('need wrapper function')

  Object.keys(fn).forEach(function (k) {
    wrapper[k] = fn[k];
  });

  return wrapper

  function wrapper() {
    var args = new Array(arguments.length);
    for (var i = 0; i < args.length; i++) {
      args[i] = arguments[i];
    }
    var ret = fn.apply(this, args);
    var cb = args[args.length-1];
    if (typeof ret === 'function' && ret !== cb) {
      Object.keys(cb).forEach(function (k) {
        ret[k] = cb[k];
      });
    }
    return ret
  }
}

var once$2 = {exports: {}};

var wrappy$1 = wrappy_1;
once$2.exports = wrappy$1(once$1);
once$2.exports.strict = wrappy$1(onceStrict);

once$1.proto = once$1(function () {
  Object.defineProperty(Function.prototype, 'once', {
    value: function () {
      return once$1(this)
    },
    configurable: true
  });

  Object.defineProperty(Function.prototype, 'onceStrict', {
    value: function () {
      return onceStrict(this)
    },
    configurable: true
  });
});

function once$1 (fn) {
  var f = function () {
    if (f.called) return f.value
    f.called = true;
    return f.value = fn.apply(this, arguments)
  };
  f.called = false;
  return f
}

function onceStrict (fn) {
  var f = function () {
    if (f.called)
      throw new Error(f.onceError)
    f.called = true;
    return f.value = fn.apply(this, arguments)
  };
  var name = fn.name || 'Function wrapped with `once`';
  f.onceError = name + " shouldn't be called more than once";
  f.called = false;
  return f
}

var wrappy = wrappy_1;
var reqs = Object.create(null);
var once = once$2.exports;

var inflight_1 = wrappy(inflight);

function inflight (key, cb) {
  if (reqs[key]) {
    reqs[key].push(cb);
    return null
  } else {
    reqs[key] = [cb];
    return makeres(key)
  }
}

function makeres (key) {
  return once(function RES () {
    var cbs = reqs[key];
    var len = cbs.length;
    var args = slice(arguments);

    // XXX It's somewhat ambiguous whether a new callback added in this
    // pass should be queued for later execution if something in the
    // list of callbacks throws, or if it should just be discarded.
    // However, it's such an edge case that it hardly matters, and either
    // choice is likely as surprising as the other.
    // As it happens, we do go ahead and schedule it for later execution.
    try {
      for (var i = 0; i < len; i++) {
        cbs[i].apply(null, args);
      }
    } finally {
      if (cbs.length > len) {
        // added more in the interim.
        // de-zalgo, just in case, but don't call again.
        cbs.splice(0, len);
        process.nextTick(function () {
          RES.apply(null, args);
        });
      } else {
        delete reqs[key];
      }
    }
  })
}

function slice (args) {
  var length = args.length;
  var array = [];

  for (var i = 0; i < length; i++) array[i] = args[i];
  return array
}

var glob_1;
var hasRequiredGlob;

function requireGlob () {
	if (hasRequiredGlob) return glob_1;
	hasRequiredGlob = 1;
	// Approach:
	//
	// 1. Get the minimatch set
	// 2. For each pattern in the set, PROCESS(pattern, false)
	// 3. Store matches per-set, then uniq them
	//
	// PROCESS(pattern, inGlobStar)
	// Get the first [n] items from pattern that are all strings
	// Join these together.  This is PREFIX.
	//   If there is no more remaining, then stat(PREFIX) and
	//   add to matches if it succeeds.  END.
	//
	// If inGlobStar and PREFIX is symlink and points to dir
	//   set ENTRIES = []
	// else readdir(PREFIX) as ENTRIES
	//   If fail, END
	//
	// with ENTRIES
	//   If pattern[n] is GLOBSTAR
	//     // handle the case where the globstar match is empty
	//     // by pruning it out, and testing the resulting pattern
	//     PROCESS(pattern[0..n] + pattern[n+1 .. $], false)
	//     // handle other cases.
	//     for ENTRY in ENTRIES (not dotfiles)
	//       // attach globstar + tail onto the entry
	//       // Mark that this entry is a globstar match
	//       PROCESS(pattern[0..n] + ENTRY + pattern[n .. $], true)
	//
	//   else // not globstar
	//     for ENTRY in ENTRIES (not dotfiles, unless pattern[n] is dot)
	//       Test ENTRY against pattern[n]
	//       If fails, continue
	//       If passes, PROCESS(pattern[0..n] + item + pattern[n+1 .. $])
	//
	// Caveat:
	//   Cache all stats and readdirs results to minimize syscall.  Since all
	//   we ever care about is existence and directory-ness, we can just keep
	//   `true` for files, and [children,...] for directories, or `false` for
	//   things that don't exist.

	glob_1 = glob;

	var rp = fs_realpath;
	var minimatch = minimatch_1;
	minimatch.Minimatch;
	var inherits$1 = inherits.exports;
	var EE = require$$3.EventEmitter;
	var path = require$$0;
	var assert = require$$5;
	var isAbsolute = pathIsAbsolute.exports;
	var globSync = requireSync();
	var common$1 = common;
	var setopts = common$1.setopts;
	var ownProp = common$1.ownProp;
	var inflight = inflight_1;
	var childrenIgnored = common$1.childrenIgnored;
	var isIgnored = common$1.isIgnored;

	var once = once$2.exports;

	function glob (pattern, options, cb) {
	  if (typeof options === 'function') cb = options, options = {};
	  if (!options) options = {};

	  if (options.sync) {
	    if (cb)
	      throw new TypeError('callback provided to sync glob')
	    return globSync(pattern, options)
	  }

	  return new Glob(pattern, options, cb)
	}

	glob.sync = globSync;
	var GlobSync = glob.GlobSync = globSync.GlobSync;

	// old api surface
	glob.glob = glob;

	function extend (origin, add) {
	  if (add === null || typeof add !== 'object') {
	    return origin
	  }

	  var keys = Object.keys(add);
	  var i = keys.length;
	  while (i--) {
	    origin[keys[i]] = add[keys[i]];
	  }
	  return origin
	}

	glob.hasMagic = function (pattern, options_) {
	  var options = extend({}, options_);
	  options.noprocess = true;

	  var g = new Glob(pattern, options);
	  var set = g.minimatch.set;

	  if (!pattern)
	    return false

	  if (set.length > 1)
	    return true

	  for (var j = 0; j < set[0].length; j++) {
	    if (typeof set[0][j] !== 'string')
	      return true
	  }

	  return false
	};

	glob.Glob = Glob;
	inherits$1(Glob, EE);
	function Glob (pattern, options, cb) {
	  if (typeof options === 'function') {
	    cb = options;
	    options = null;
	  }

	  if (options && options.sync) {
	    if (cb)
	      throw new TypeError('callback provided to sync glob')
	    return new GlobSync(pattern, options)
	  }

	  if (!(this instanceof Glob))
	    return new Glob(pattern, options, cb)

	  setopts(this, pattern, options);
	  this._didRealPath = false;

	  // process each pattern in the minimatch set
	  var n = this.minimatch.set.length;

	  // The matches are stored as {<filename>: true,...} so that
	  // duplicates are automagically pruned.
	  // Later, we do an Object.keys() on these.
	  // Keep them as a list so we can fill in when nonull is set.
	  this.matches = new Array(n);

	  if (typeof cb === 'function') {
	    cb = once(cb);
	    this.on('error', cb);
	    this.on('end', function (matches) {
	      cb(null, matches);
	    });
	  }

	  var self = this;
	  this._processing = 0;

	  this._emitQueue = [];
	  this._processQueue = [];
	  this.paused = false;

	  if (this.noprocess)
	    return this

	  if (n === 0)
	    return done()

	  var sync = true;
	  for (var i = 0; i < n; i ++) {
	    this._process(this.minimatch.set[i], i, false, done);
	  }
	  sync = false;

	  function done () {
	    --self._processing;
	    if (self._processing <= 0) {
	      if (sync) {
	        process.nextTick(function () {
	          self._finish();
	        });
	      } else {
	        self._finish();
	      }
	    }
	  }
	}

	Glob.prototype._finish = function () {
	  assert(this instanceof Glob);
	  if (this.aborted)
	    return

	  if (this.realpath && !this._didRealpath)
	    return this._realpath()

	  common$1.finish(this);
	  this.emit('end', this.found);
	};

	Glob.prototype._realpath = function () {
	  if (this._didRealpath)
	    return

	  this._didRealpath = true;

	  var n = this.matches.length;
	  if (n === 0)
	    return this._finish()

	  var self = this;
	  for (var i = 0; i < this.matches.length; i++)
	    this._realpathSet(i, next);

	  function next () {
	    if (--n === 0)
	      self._finish();
	  }
	};

	Glob.prototype._realpathSet = function (index, cb) {
	  var matchset = this.matches[index];
	  if (!matchset)
	    return cb()

	  var found = Object.keys(matchset);
	  var self = this;
	  var n = found.length;

	  if (n === 0)
	    return cb()

	  var set = this.matches[index] = Object.create(null);
	  found.forEach(function (p, i) {
	    // If there's a problem with the stat, then it means that
	    // one or more of the links in the realpath couldn't be
	    // resolved.  just return the abs value in that case.
	    p = self._makeAbs(p);
	    rp.realpath(p, self.realpathCache, function (er, real) {
	      if (!er)
	        set[real] = true;
	      else if (er.syscall === 'stat')
	        set[p] = true;
	      else
	        self.emit('error', er); // srsly wtf right here

	      if (--n === 0) {
	        self.matches[index] = set;
	        cb();
	      }
	    });
	  });
	};

	Glob.prototype._mark = function (p) {
	  return common$1.mark(this, p)
	};

	Glob.prototype._makeAbs = function (f) {
	  return common$1.makeAbs(this, f)
	};

	Glob.prototype.abort = function () {
	  this.aborted = true;
	  this.emit('abort');
	};

	Glob.prototype.pause = function () {
	  if (!this.paused) {
	    this.paused = true;
	    this.emit('pause');
	  }
	};

	Glob.prototype.resume = function () {
	  if (this.paused) {
	    this.emit('resume');
	    this.paused = false;
	    if (this._emitQueue.length) {
	      var eq = this._emitQueue.slice(0);
	      this._emitQueue.length = 0;
	      for (var i = 0; i < eq.length; i ++) {
	        var e = eq[i];
	        this._emitMatch(e[0], e[1]);
	      }
	    }
	    if (this._processQueue.length) {
	      var pq = this._processQueue.slice(0);
	      this._processQueue.length = 0;
	      for (var i = 0; i < pq.length; i ++) {
	        var p = pq[i];
	        this._processing--;
	        this._process(p[0], p[1], p[2], p[3]);
	      }
	    }
	  }
	};

	Glob.prototype._process = function (pattern, index, inGlobStar, cb) {
	  assert(this instanceof Glob);
	  assert(typeof cb === 'function');

	  if (this.aborted)
	    return

	  this._processing++;
	  if (this.paused) {
	    this._processQueue.push([pattern, index, inGlobStar, cb]);
	    return
	  }

	  //console.error('PROCESS %d', this._processing, pattern)

	  // Get the first [n] parts of pattern that are all strings.
	  var n = 0;
	  while (typeof pattern[n] === 'string') {
	    n ++;
	  }
	  // now n is the index of the first one that is *not* a string.

	  // see if there's anything else
	  var prefix;
	  switch (n) {
	    // if not, then this is rather simple
	    case pattern.length:
	      this._processSimple(pattern.join('/'), index, cb);
	      return

	    case 0:
	      // pattern *starts* with some non-trivial item.
	      // going to readdir(cwd), but not include the prefix in matches.
	      prefix = null;
	      break

	    default:
	      // pattern has some string bits in the front.
	      // whatever it starts with, whether that's 'absolute' like /foo/bar,
	      // or 'relative' like '../baz'
	      prefix = pattern.slice(0, n).join('/');
	      break
	  }

	  var remain = pattern.slice(n);

	  // get the list of entries.
	  var read;
	  if (prefix === null)
	    read = '.';
	  else if (isAbsolute(prefix) ||
	      isAbsolute(pattern.map(function (p) {
	        return typeof p === 'string' ? p : '[*]'
	      }).join('/'))) {
	    if (!prefix || !isAbsolute(prefix))
	      prefix = '/' + prefix;
	    read = prefix;
	  } else
	    read = prefix;

	  var abs = this._makeAbs(read);

	  //if ignored, skip _processing
	  if (childrenIgnored(this, read))
	    return cb()

	  var isGlobStar = remain[0] === minimatch.GLOBSTAR;
	  if (isGlobStar)
	    this._processGlobStar(prefix, read, abs, remain, index, inGlobStar, cb);
	  else
	    this._processReaddir(prefix, read, abs, remain, index, inGlobStar, cb);
	};

	Glob.prototype._processReaddir = function (prefix, read, abs, remain, index, inGlobStar, cb) {
	  var self = this;
	  this._readdir(abs, inGlobStar, function (er, entries) {
	    return self._processReaddir2(prefix, read, abs, remain, index, inGlobStar, entries, cb)
	  });
	};

	Glob.prototype._processReaddir2 = function (prefix, read, abs, remain, index, inGlobStar, entries, cb) {

	  // if the abs isn't a dir, then nothing can match!
	  if (!entries)
	    return cb()

	  // It will only match dot entries if it starts with a dot, or if
	  // dot is set.  Stuff like @(.foo|.bar) isn't allowed.
	  var pn = remain[0];
	  var negate = !!this.minimatch.negate;
	  var rawGlob = pn._glob;
	  var dotOk = this.dot || rawGlob.charAt(0) === '.';

	  var matchedEntries = [];
	  for (var i = 0; i < entries.length; i++) {
	    var e = entries[i];
	    if (e.charAt(0) !== '.' || dotOk) {
	      var m;
	      if (negate && !prefix) {
	        m = !e.match(pn);
	      } else {
	        m = e.match(pn);
	      }
	      if (m)
	        matchedEntries.push(e);
	    }
	  }

	  //console.error('prd2', prefix, entries, remain[0]._glob, matchedEntries)

	  var len = matchedEntries.length;
	  // If there are no matched entries, then nothing matches.
	  if (len === 0)
	    return cb()

	  // if this is the last remaining pattern bit, then no need for
	  // an additional stat *unless* the user has specified mark or
	  // stat explicitly.  We know they exist, since readdir returned
	  // them.

	  if (remain.length === 1 && !this.mark && !this.stat) {
	    if (!this.matches[index])
	      this.matches[index] = Object.create(null);

	    for (var i = 0; i < len; i ++) {
	      var e = matchedEntries[i];
	      if (prefix) {
	        if (prefix !== '/')
	          e = prefix + '/' + e;
	        else
	          e = prefix + e;
	      }

	      if (e.charAt(0) === '/' && !this.nomount) {
	        e = path.join(this.root, e);
	      }
	      this._emitMatch(index, e);
	    }
	    // This was the last one, and no stats were needed
	    return cb()
	  }

	  // now test all matched entries as stand-ins for that part
	  // of the pattern.
	  remain.shift();
	  for (var i = 0; i < len; i ++) {
	    var e = matchedEntries[i];
	    if (prefix) {
	      if (prefix !== '/')
	        e = prefix + '/' + e;
	      else
	        e = prefix + e;
	    }
	    this._process([e].concat(remain), index, inGlobStar, cb);
	  }
	  cb();
	};

	Glob.prototype._emitMatch = function (index, e) {
	  if (this.aborted)
	    return

	  if (isIgnored(this, e))
	    return

	  if (this.paused) {
	    this._emitQueue.push([index, e]);
	    return
	  }

	  var abs = isAbsolute(e) ? e : this._makeAbs(e);

	  if (this.mark)
	    e = this._mark(e);

	  if (this.absolute)
	    e = abs;

	  if (this.matches[index][e])
	    return

	  if (this.nodir) {
	    var c = this.cache[abs];
	    if (c === 'DIR' || Array.isArray(c))
	      return
	  }

	  this.matches[index][e] = true;

	  var st = this.statCache[abs];
	  if (st)
	    this.emit('stat', e, st);

	  this.emit('match', e);
	};

	Glob.prototype._readdirInGlobStar = function (abs, cb) {
	  if (this.aborted)
	    return

	  // follow all symlinked directories forever
	  // just proceed as if this is a non-globstar situation
	  if (this.follow)
	    return this._readdir(abs, false, cb)

	  var lstatkey = 'lstat\0' + abs;
	  var self = this;
	  var lstatcb = inflight(lstatkey, lstatcb_);

	  if (lstatcb)
	    self.fs.lstat(abs, lstatcb);

	  function lstatcb_ (er, lstat) {
	    if (er && er.code === 'ENOENT')
	      return cb()

	    var isSym = lstat && lstat.isSymbolicLink();
	    self.symlinks[abs] = isSym;

	    // If it's not a symlink or a dir, then it's definitely a regular file.
	    // don't bother doing a readdir in that case.
	    if (!isSym && lstat && !lstat.isDirectory()) {
	      self.cache[abs] = 'FILE';
	      cb();
	    } else
	      self._readdir(abs, false, cb);
	  }
	};

	Glob.prototype._readdir = function (abs, inGlobStar, cb) {
	  if (this.aborted)
	    return

	  cb = inflight('readdir\0'+abs+'\0'+inGlobStar, cb);
	  if (!cb)
	    return

	  //console.error('RD %j %j', +inGlobStar, abs)
	  if (inGlobStar && !ownProp(this.symlinks, abs))
	    return this._readdirInGlobStar(abs, cb)

	  if (ownProp(this.cache, abs)) {
	    var c = this.cache[abs];
	    if (!c || c === 'FILE')
	      return cb()

	    if (Array.isArray(c))
	      return cb(null, c)
	  }

	  var self = this;
	  self.fs.readdir(abs, readdirCb(this, abs, cb));
	};

	function readdirCb (self, abs, cb) {
	  return function (er, entries) {
	    if (er)
	      self._readdirError(abs, er, cb);
	    else
	      self._readdirEntries(abs, entries, cb);
	  }
	}

	Glob.prototype._readdirEntries = function (abs, entries, cb) {
	  if (this.aborted)
	    return

	  // if we haven't asked to stat everything, then just
	  // assume that everything in there exists, so we can avoid
	  // having to stat it a second time.
	  if (!this.mark && !this.stat) {
	    for (var i = 0; i < entries.length; i ++) {
	      var e = entries[i];
	      if (abs === '/')
	        e = abs + e;
	      else
	        e = abs + '/' + e;
	      this.cache[e] = true;
	    }
	  }

	  this.cache[abs] = entries;
	  return cb(null, entries)
	};

	Glob.prototype._readdirError = function (f, er, cb) {
	  if (this.aborted)
	    return

	  // handle errors, and cache the information
	  switch (er.code) {
	    case 'ENOTSUP': // https://github.com/isaacs/node-glob/issues/205
	    case 'ENOTDIR': // totally normal. means it *does* exist.
	      var abs = this._makeAbs(f);
	      this.cache[abs] = 'FILE';
	      if (abs === this.cwdAbs) {
	        var error = new Error(er.code + ' invalid cwd ' + this.cwd);
	        error.path = this.cwd;
	        error.code = er.code;
	        this.emit('error', error);
	        this.abort();
	      }
	      break

	    case 'ENOENT': // not terribly unusual
	    case 'ELOOP':
	    case 'ENAMETOOLONG':
	    case 'UNKNOWN':
	      this.cache[this._makeAbs(f)] = false;
	      break

	    default: // some unusual error.  Treat as failure.
	      this.cache[this._makeAbs(f)] = false;
	      if (this.strict) {
	        this.emit('error', er);
	        // If the error is handled, then we abort
	        // if not, we threw out of here
	        this.abort();
	      }
	      if (!this.silent)
	        console.error('glob error', er);
	      break
	  }

	  return cb()
	};

	Glob.prototype._processGlobStar = function (prefix, read, abs, remain, index, inGlobStar, cb) {
	  var self = this;
	  this._readdir(abs, inGlobStar, function (er, entries) {
	    self._processGlobStar2(prefix, read, abs, remain, index, inGlobStar, entries, cb);
	  });
	};


	Glob.prototype._processGlobStar2 = function (prefix, read, abs, remain, index, inGlobStar, entries, cb) {
	  //console.error('pgs2', prefix, remain[0], entries)

	  // no entries means not a dir, so it can never have matches
	  // foo.txt/** doesn't match foo.txt
	  if (!entries)
	    return cb()

	  // test without the globstar, and with every child both below
	  // and replacing the globstar.
	  var remainWithoutGlobStar = remain.slice(1);
	  var gspref = prefix ? [ prefix ] : [];
	  var noGlobStar = gspref.concat(remainWithoutGlobStar);

	  // the noGlobStar pattern exits the inGlobStar state
	  this._process(noGlobStar, index, false, cb);

	  var isSym = this.symlinks[abs];
	  var len = entries.length;

	  // If it's a symlink, and we're in a globstar, then stop
	  if (isSym && inGlobStar)
	    return cb()

	  for (var i = 0; i < len; i++) {
	    var e = entries[i];
	    if (e.charAt(0) === '.' && !this.dot)
	      continue

	    // these two cases enter the inGlobStar state
	    var instead = gspref.concat(entries[i], remainWithoutGlobStar);
	    this._process(instead, index, true, cb);

	    var below = gspref.concat(entries[i], remain);
	    this._process(below, index, true, cb);
	  }

	  cb();
	};

	Glob.prototype._processSimple = function (prefix, index, cb) {
	  // XXX review this.  Shouldn't it be doing the mounting etc
	  // before doing stat?  kinda weird?
	  var self = this;
	  this._stat(prefix, function (er, exists) {
	    self._processSimple2(prefix, index, er, exists, cb);
	  });
	};
	Glob.prototype._processSimple2 = function (prefix, index, er, exists, cb) {

	  //console.error('ps2', prefix, exists)

	  if (!this.matches[index])
	    this.matches[index] = Object.create(null);

	  // If it doesn't exist, then just mark the lack of results
	  if (!exists)
	    return cb()

	  if (prefix && isAbsolute(prefix) && !this.nomount) {
	    var trail = /[\/\\]$/.test(prefix);
	    if (prefix.charAt(0) === '/') {
	      prefix = path.join(this.root, prefix);
	    } else {
	      prefix = path.resolve(this.root, prefix);
	      if (trail)
	        prefix += '/';
	    }
	  }

	  if (process.platform === 'win32')
	    prefix = prefix.replace(/\\/g, '/');

	  // Mark this as a match
	  this._emitMatch(index, prefix);
	  cb();
	};

	// Returns either 'DIR', 'FILE', or false
	Glob.prototype._stat = function (f, cb) {
	  var abs = this._makeAbs(f);
	  var needDir = f.slice(-1) === '/';

	  if (f.length > this.maxLength)
	    return cb()

	  if (!this.stat && ownProp(this.cache, abs)) {
	    var c = this.cache[abs];

	    if (Array.isArray(c))
	      c = 'DIR';

	    // It exists, but maybe not how we need it
	    if (!needDir || c === 'DIR')
	      return cb(null, c)

	    if (needDir && c === 'FILE')
	      return cb()

	    // otherwise we have to stat, because maybe c=true
	    // if we know it exists, but not what it is.
	  }
	  var stat = this.statCache[abs];
	  if (stat !== undefined) {
	    if (stat === false)
	      return cb(null, stat)
	    else {
	      var type = stat.isDirectory() ? 'DIR' : 'FILE';
	      if (needDir && type === 'FILE')
	        return cb()
	      else
	        return cb(null, type, stat)
	    }
	  }

	  var self = this;
	  var statcb = inflight('stat\0' + abs, lstatcb_);
	  if (statcb)
	    self.fs.lstat(abs, statcb);

	  function lstatcb_ (er, lstat) {
	    if (lstat && lstat.isSymbolicLink()) {
	      // If it's a symlink, then treat it as the target, unless
	      // the target does not exist, then treat it as a file.
	      return self.fs.stat(abs, function (er, stat) {
	        if (er)
	          self._stat2(f, abs, null, lstat, cb);
	        else
	          self._stat2(f, abs, er, stat, cb);
	      })
	    } else {
	      self._stat2(f, abs, er, lstat, cb);
	    }
	  }
	};

	Glob.prototype._stat2 = function (f, abs, er, stat, cb) {
	  if (er && (er.code === 'ENOENT' || er.code === 'ENOTDIR')) {
	    this.statCache[abs] = false;
	    return cb()
	  }

	  var needDir = f.slice(-1) === '/';
	  this.statCache[abs] = stat;

	  if (abs.slice(-1) === '/' && stat && !stat.isDirectory())
	    return cb(null, false, stat)

	  var c = true;
	  if (stat)
	    c = stat.isDirectory() ? 'DIR' : 'FILE';
	  this.cache[abs] = this.cache[abs] || c;

	  if (needDir && c === 'FILE')
	    return cb()

	  return cb(null, c, stat)
	};
	return glob_1;
}

var defaultExtension$2 = [
	'.js',
	'.cjs',
	'.mjs',
	'.ts',
	'.tsx',
	'.jsx'
];

const defaultExtension$1 = defaultExtension$2;
const testFileExtensions = defaultExtension$1
	.map(extension => extension.slice(1))
	.join(',');

var defaultExclude$1 = [
	'coverage/**',
	'packages/*/test{,s}/**',
	'**/*.d.ts',
	'test{,s}/**',
	`test{,-*}.{${testFileExtensions}}`,
	`**/*{.,-}test.{${testFileExtensions}}`,
	'**/__tests__/**',

	/* Exclude common development tool configuration files */
	'**/{ava,babel,nyc}.config.{js,cjs,mjs}',
	'**/jest.config.{js,cjs,mjs,ts}',
	'**/{karma,rollup,webpack}.config.js',
	'**/.{eslint,mocha}rc.{js,cjs}'
];

const defaultExclude = defaultExclude$1;
const defaultExtension = defaultExtension$2;

const nycCommands = {
	all: [null, 'check-coverage', 'instrument', 'merge', 'report'],
	testExclude: [null, 'instrument', 'report', 'check-coverage'],
	instrument: [null, 'instrument'],
	checkCoverage: [null, 'report', 'check-coverage'],
	report: [null, 'report'],
	main: [null],
	instrumentOnly: ['instrument']
};

const cwd = {
	description: 'working directory used when resolving paths',
	type: 'string',
	get default() {
		return process.cwd();
	},
	nycCommands: nycCommands.all
};

const nycrcPath = {
	description: 'specify an explicit path to find nyc configuration',
	nycCommands: nycCommands.all
};

const tempDir = {
	description: 'directory to output raw coverage information to',
	type: 'string',
	default: './.nyc_output',
	nycAlias: 't',
	nycHiddenAlias: 'temp-directory',
	nycCommands: [null, 'check-coverage', 'merge', 'report']
};

const testExclude$1 = {
	exclude: {
		description: 'a list of specific files and directories that should be excluded from coverage, glob patterns are supported',
		type: 'array',
		items: {
			type: 'string'
		},
		default: defaultExclude,
		nycCommands: nycCommands.testExclude,
		nycAlias: 'x'
	},
	excludeNodeModules: {
		description: 'whether or not to exclude all node_module folders (i.e. **/node_modules/**) by default',
		type: 'boolean',
		default: true,
		nycCommands: nycCommands.testExclude
	},
	include: {
		description: 'a list of specific files that should be covered, glob patterns are supported',
		type: 'array',
		items: {
			type: 'string'
		},
		default: [],
		nycCommands: nycCommands.testExclude,
		nycAlias: 'n'
	},
	extension: {
		description: 'a list of extensions that nyc should handle in addition to .js',
		type: 'array',
		items: {
			type: 'string'
		},
		default: defaultExtension,
		nycCommands: nycCommands.testExclude,
		nycAlias: 'e'
	}
};

const instrumentVisitor = {
	coverageVariable: {
		description: 'variable to store coverage',
		type: 'string',
		default: '__coverage__',
		nycCommands: nycCommands.instrument
	},
	coverageGlobalScope: {
		description: 'scope to store the coverage variable',
		type: 'string',
		default: 'this',
		nycCommands: nycCommands.instrument
	},
	coverageGlobalScopeFunc: {
		description: 'avoid potentially replaced `Function` when finding global scope',
		type: 'boolean',
		default: true,
		nycCommands: nycCommands.instrument
	},
	ignoreClassMethods: {
		description: 'class method names to ignore for coverage',
		type: 'array',
		items: {
			type: 'string'
		},
		default: [],
		nycCommands: nycCommands.instrument
	}
};

const instrumentParseGen = {
	autoWrap: {
		description: 'allow `return` statements outside of functions',
		type: 'boolean',
		default: true,
		nycCommands: nycCommands.instrument
	},
	esModules: {
		description: 'should files be treated as ES Modules',
		type: 'boolean',
		default: true,
		nycCommands: nycCommands.instrument
	},
	parserPlugins: {
		description: 'babel parser plugins to use when parsing the source',
		type: 'array',
		items: {
			type: 'string'
		},
		/* Babel parser plugins are to be enabled when the feature is stage 3 and
		 * implemented in a released version of node.js. */
		default: [
			'asyncGenerators',
			'bigInt',
			'classProperties',
			'classPrivateProperties',
			'classPrivateMethods',
			'dynamicImport',
			'importMeta',
			'numericSeparator',
			'objectRestSpread',
			'optionalCatchBinding',
			'topLevelAwait'
		],
		nycCommands: nycCommands.instrument
	},
	compact: {
		description: 'should the output be compacted?',
		type: 'boolean',
		default: true,
		nycCommands: nycCommands.instrument
	},
	preserveComments: {
		description: 'should comments be preserved in the output?',
		type: 'boolean',
		default: true,
		nycCommands: nycCommands.instrument
	},
	produceSourceMap: {
		description: 'should source maps be produced?',
		type: 'boolean',
		default: true,
		nycCommands: nycCommands.instrument
	}
};

const checkCoverage = {
	excludeAfterRemap: {
		description: 'should exclude logic be performed after the source-map remaps filenames?',
		type: 'boolean',
		default: true,
		nycCommands: nycCommands.checkCoverage
	},
	branches: {
		description: 'what % of branches must be covered?',
		type: 'number',
		default: 0,
		minimum: 0,
		maximum: 100,
		nycCommands: nycCommands.checkCoverage
	},
	functions: {
		description: 'what % of functions must be covered?',
		type: 'number',
		default: 0,
		minimum: 0,
		maximum: 100,
		nycCommands: nycCommands.checkCoverage
	},
	lines: {
		description: 'what % of lines must be covered?',
		type: 'number',
		default: 90,
		minimum: 0,
		maximum: 100,
		nycCommands: nycCommands.checkCoverage
	},
	statements: {
		description: 'what % of statements must be covered?',
		type: 'number',
		default: 0,
		minimum: 0,
		maximum: 100,
		nycCommands: nycCommands.checkCoverage
	},
	perFile: {
		description: 'check thresholds per file',
		type: 'boolean',
		default: false,
		nycCommands: nycCommands.checkCoverage
	}
};

const report$1 = {
	checkCoverage: {
		description: 'check whether coverage is within thresholds provided',
		type: 'boolean',
		default: false,
		nycCommands: nycCommands.report
	},
	reporter: {
		description: 'coverage reporter(s) to use',
		type: 'array',
		items: {
			type: 'string'
		},
		default: ['text'],
		nycCommands: nycCommands.report,
		nycAlias: 'r'
	},
	reportDir: {
		description: 'directory to output coverage reports in',
		type: 'string',
		default: 'coverage',
		nycCommands: nycCommands.report
	},
	showProcessTree: {
		description: 'display the tree of spawned processes',
		type: 'boolean',
		default: false,
		nycCommands: nycCommands.report
	},
	skipEmpty: {
		description: 'don\'t show empty files (no lines of code) in report',
		type: 'boolean',
		default: false,
		nycCommands: nycCommands.report
	},
	skipFull: {
		description: 'don\'t show files with 100% statement, branch, and function coverage',
		type: 'boolean',
		default: false,
		nycCommands: nycCommands.report
	}
};

const nycMain = {
	silent: {
		description: 'don\'t output a report after tests finish running',
		type: 'boolean',
		default: false,
		nycCommands: nycCommands.main,
		nycAlias: 's'
	},
	all: {
		description: 'whether or not to instrument all files of the project (not just the ones touched by your test suite)',
		type: 'boolean',
		default: false,
		nycCommands: nycCommands.main,
		nycAlias: 'a'
	},
	eager: {
		description: 'instantiate the instrumenter at startup (see https://git.io/vMKZ9)',
		type: 'boolean',
		default: false,
		nycCommands: nycCommands.main
	},
	cache: {
		description: 'cache instrumentation results for improved performance',
		type: 'boolean',
		default: true,
		nycCommands: nycCommands.main,
		nycAlias: 'c'
	},
	cacheDir: {
		description: 'explicitly set location for instrumentation cache',
		type: 'string',
		nycCommands: nycCommands.main
	},
	babelCache: {
		description: 'cache babel transpilation results for improved performance',
		type: 'boolean',
		default: false,
		nycCommands: nycCommands.main
	},
	useSpawnWrap: {
		description: 'use spawn-wrap instead of setting process.env.NODE_OPTIONS',
		type: 'boolean',
		default: false,
		nycCommands: nycCommands.main
	},
	hookRequire: {
		description: 'should nyc wrap require?',
		type: 'boolean',
		default: true,
		nycCommands: nycCommands.main
	},
	hookRunInContext: {
		description: 'should nyc wrap vm.runInContext?',
		type: 'boolean',
		default: false,
		nycCommands: nycCommands.main
	},
	hookRunInThisContext: {
		description: 'should nyc wrap vm.runInThisContext?',
		type: 'boolean',
		default: false,
		nycCommands: nycCommands.main
	},
	clean: {
		description: 'should the .nyc_output folder be cleaned before executing tests',
		type: 'boolean',
		default: true,
		nycCommands: nycCommands.main
	}
};

const instrumentOnly = {
	inPlace: {
		description: 'should nyc run the instrumentation in place?',
		type: 'boolean',
		default: false,
		nycCommands: nycCommands.instrumentOnly
	},
	exitOnError: {
		description: 'should nyc exit when an instrumentation failure occurs?',
		type: 'boolean',
		default: false,
		nycCommands: nycCommands.instrumentOnly
	},
	delete: {
		description: 'should the output folder be deleted before instrumenting files?',
		type: 'boolean',
		default: false,
		nycCommands: nycCommands.instrumentOnly
	},
	completeCopy: {
		description: 'should nyc copy all files from input to output as well as instrumented files?',
		type: 'boolean',
		default: false,
		nycCommands: nycCommands.instrumentOnly
	}
};

const nyc = {
	description: 'nyc configuration options',
	type: 'object',
	properties: {
		cwd,
		nycrcPath,
		tempDir,

		/* Test Exclude */
		...testExclude$1,

		/* Instrumentation settings */
		...instrumentVisitor,

		/* Instrumentation parser/generator settings */
		...instrumentParseGen,
		sourceMap: {
			description: 'should nyc detect and handle source maps?',
			type: 'boolean',
			default: true,
			nycCommands: nycCommands.instrument
		},
		require: {
			description: 'a list of additional modules that nyc should attempt to require in its subprocess, e.g., @babel/register, @babel/polyfill',
			type: 'array',
			items: {
				type: 'string'
			},
			default: [],
			nycCommands: nycCommands.instrument,
			nycAlias: 'i'
		},
		instrument: {
			description: 'should nyc handle instrumentation?',
			type: 'boolean',
			default: true,
			nycCommands: nycCommands.instrument
		},

		/* Check coverage */
		...checkCoverage,

		/* Report options */
		...report$1,

		/* Main command options */
		...nycMain,

		/* Instrument command options */
		...instrumentOnly
	}
};

const configs = {
	nyc,
	testExclude: {
		description: 'test-exclude options',
		type: 'object',
		properties: {
			cwd,
			...testExclude$1
		}
	},
	babelPluginIstanbul: {
		description: 'babel-plugin-istanbul options',
		type: 'object',
		properties: {
			cwd,
			...testExclude$1,
			...instrumentVisitor
		}
	},
	instrumentVisitor: {
		description: 'instrument visitor options',
		type: 'object',
		properties: instrumentVisitor
	},
	instrumenter: {
		description: 'stand-alone instrumenter options',
		type: 'object',
		properties: {
			...instrumentVisitor,
			...instrumentParseGen
		}
	}
};

function defaultsReducer(defaults, [name, {default: value}]) {
	/* Modifying arrays in defaults is safe, does not change schema. */
	if (Array.isArray(value)) {
		value = [...value];
	}

	return Object.assign(defaults, {[name]: value});
}

var schema = {
	...configs,
	defaults: Object.keys(configs).reduce(
		(defaults, id) => {
			Object.defineProperty(defaults, id, {
				enumerable: true,
				get() {
					/* This defers `process.cwd()` until defaults are requested. */
					return Object.entries(configs[id].properties)
						.filter(([, info]) => 'default' in info)
						.reduce(defaultsReducer, {});
				}
			});

			return defaults;
		},
		{}
	)
};

var isOutsideDir$1 = {exports: {}};

var isOutsideDirWin32;
var hasRequiredIsOutsideDirWin32;

function requireIsOutsideDirWin32 () {
	if (hasRequiredIsOutsideDirWin32) return isOutsideDirWin32;
	hasRequiredIsOutsideDirWin32 = 1;

	const path = require$$0;
	const minimatch = minimatch_1;

	const dot = { dot: true };

	isOutsideDirWin32 = function(dir, filename) {
	    return !minimatch(path.resolve(dir, filename), path.join(dir, '**'), dot);
	};
	return isOutsideDirWin32;
}

var isOutsideDirPosix;
var hasRequiredIsOutsideDirPosix;

function requireIsOutsideDirPosix () {
	if (hasRequiredIsOutsideDirPosix) return isOutsideDirPosix;
	hasRequiredIsOutsideDirPosix = 1;

	const path = require$$0;

	isOutsideDirPosix = function(dir, filename) {
	    return /^\.\./.test(path.relative(dir, filename));
	};
	return isOutsideDirPosix;
}

(function (module) {

	if (process.platform === 'win32') {
	    module.exports = requireIsOutsideDirWin32();
	} else {
	    module.exports = requireIsOutsideDirPosix();
	}
} (isOutsideDir$1));

const path$3 = require$$0;
const { promisify: promisify$1 } = require$$2;
const glob = promisify$1(requireGlob());
const minimatch = minimatch_1;
const { defaults } = schema;
const isOutsideDir = isOutsideDir$1.exports;

class TestExclude {
    constructor(opts = {}) {
        Object.assign(
            this,
            {relativePath: true},
            defaults.testExclude
        );

        for (const [name, value] of Object.entries(opts)) {
            if (value !== undefined) {
                this[name] = value;
            }
        }

        if (typeof this.include === 'string') {
            this.include = [this.include];
        }

        if (typeof this.exclude === 'string') {
            this.exclude = [this.exclude];
        }

        if (typeof this.extension === 'string') {
            this.extension = [this.extension];
        } else if (this.extension.length === 0) {
            this.extension = false;
        }

        if (this.include && this.include.length > 0) {
            this.include = prepGlobPatterns([].concat(this.include));
        } else {
            this.include = false;
        }

        if (
            this.excludeNodeModules &&
            !this.exclude.includes('**/node_modules/**')
        ) {
            this.exclude = this.exclude.concat('**/node_modules/**');
        }

        this.exclude = prepGlobPatterns([].concat(this.exclude));

        this.handleNegation();
    }

    /* handle the special case of negative globs
     * (!**foo/bar); we create a new this.excludeNegated set
     * of rules, which is applied after excludes and we
     * move excluded include rules into this.excludes.
     */
    handleNegation() {
        const noNeg = e => e.charAt(0) !== '!';
        const onlyNeg = e => e.charAt(0) === '!';
        const stripNeg = e => e.slice(1);

        if (Array.isArray(this.include)) {
            const includeNegated = this.include.filter(onlyNeg).map(stripNeg);
            this.exclude.push(...prepGlobPatterns(includeNegated));
            this.include = this.include.filter(noNeg);
        }

        this.excludeNegated = this.exclude.filter(onlyNeg).map(stripNeg);
        this.exclude = this.exclude.filter(noNeg);
        this.excludeNegated = prepGlobPatterns(this.excludeNegated);
    }

    shouldInstrument(filename, relFile) {
        if (
            this.extension &&
            !this.extension.some(ext => filename.endsWith(ext))
        ) {
            return false;
        }

        let pathToCheck = filename;

        if (this.relativePath) {
            relFile = relFile || path$3.relative(this.cwd, filename);

            // Don't instrument files that are outside of the current working directory.
            if (isOutsideDir(this.cwd, filename)) {
                return false;
            }

            pathToCheck = relFile.replace(/^\.[\\/]/, ''); // remove leading './' or '.\'.
        }

        const dot = { dot: true };
        const matches = pattern => minimatch(pathToCheck, pattern, dot);
        return (
            (!this.include || this.include.some(matches)) &&
            (!this.exclude.some(matches) || this.excludeNegated.some(matches))
        );
    }

    globSync(cwd = this.cwd) {
        const globPatterns = getExtensionPattern(this.extension || []);
        const globOptions = { cwd, nodir: true, dot: true };
        /* If we don't have any excludeNegated then we can optimize glob by telling
         * it to not iterate into unwanted directory trees (like node_modules). */
        if (this.excludeNegated.length === 0) {
            globOptions.ignore = this.exclude;
        }

        return glob
            .sync(globPatterns, globOptions)
            .filter(file => this.shouldInstrument(path$3.resolve(cwd, file)));
    }

    async glob(cwd = this.cwd) {
        const globPatterns = getExtensionPattern(this.extension || []);
        const globOptions = { cwd, nodir: true, dot: true };
        /* If we don't have any excludeNegated then we can optimize glob by telling
         * it to not iterate into unwanted directory trees (like node_modules). */
        if (this.excludeNegated.length === 0) {
            globOptions.ignore = this.exclude;
        }

        const list = await glob(globPatterns, globOptions);
        return list.filter(file => this.shouldInstrument(path$3.resolve(cwd, file)));
    }
}

function prepGlobPatterns(patterns) {
    return patterns.reduce((result, pattern) => {
        // Allow gitignore style of directory exclusion
        if (!/\/\*\*$/.test(pattern)) {
            result = result.concat(pattern.replace(/\/$/, '') + '/**');
        }

        // Any rules of the form **/foo.js, should also match foo.js.
        if (/^\*\*\//.test(pattern)) {
            result = result.concat(pattern.replace(/^\*\*\//, ''));
        }

        return result.concat(pattern);
    }, []);
}

function getExtensionPattern(extension) {
    switch (extension.length) {
        case 0:
            return '**';
        case 1:
            return `**/*${extension[0]}`;
        default:
            return `**/*{${extension.join()}}`;
    }
}

var testExclude = TestExclude;

var istanbulLibCoverage = {exports: {}};

/*
 Copyright 2012-2015, Yahoo Inc.
 Copyrights licensed under the New BSD License. See the accompanying LICENSE file for terms.
 */

var percent$2 = function percent(covered, total) {
    let tmp;
    if (total > 0) {
        tmp = (1000 * 100 * covered) / total;
        return Math.floor(tmp / 10) / 100;
    } else {
        return 100.0;
    }
};

var dataProperties$2 = function dataProperties(klass, properties) {
    properties.forEach(p => {
        Object.defineProperty(klass.prototype, p, {
            enumerable: true,
            get() {
                return this.data[p];
            }
        });
    });
};

/*
 Copyright 2012-2015, Yahoo Inc.
 Copyrights licensed under the New BSD License. See the accompanying LICENSE file for terms.
 */

const percent$1 = percent$2;
const dataProperties$1 = dataProperties$2;

function blankSummary() {
    const empty = () => ({
        total: 0,
        covered: 0,
        skipped: 0,
        pct: 'Unknown'
    });

    return {
        lines: empty(),
        statements: empty(),
        functions: empty(),
        branches: empty(),
        branchesTrue: empty()
    };
}

// asserts that a data object "looks like" a summary coverage object
function assertValidSummary(obj) {
    const valid =
        obj && obj.lines && obj.statements && obj.functions && obj.branches;
    if (!valid) {
        throw new Error(
            'Invalid summary coverage object, missing keys, found:' +
                Object.keys(obj).join(',')
        );
    }
}

/**
 * CoverageSummary provides a summary of code coverage . It exposes 4 properties,
 * `lines`, `statements`, `branches`, and `functions`. Each of these properties
 * is an object that has 4 keys `total`, `covered`, `skipped` and `pct`.
 * `pct` is a percentage number (0-100).
 */
let CoverageSummary$3 = class CoverageSummary {
    /**
     * @constructor
     * @param {Object|CoverageSummary} [obj=undefined] an optional data object or
     * another coverage summary to initialize this object with.
     */
    constructor(obj) {
        if (!obj) {
            this.data = blankSummary();
        } else if (obj instanceof CoverageSummary$3) {
            this.data = obj.data;
        } else {
            this.data = obj;
        }
        assertValidSummary(this.data);
    }

    /**
     * merges a second summary coverage object into this one
     * @param {CoverageSummary} obj - another coverage summary object
     */
    merge(obj) {
        const keys = [
            'lines',
            'statements',
            'branches',
            'functions',
            'branchesTrue'
        ];
        keys.forEach(key => {
            if (obj[key]) {
                this[key].total += obj[key].total;
                this[key].covered += obj[key].covered;
                this[key].skipped += obj[key].skipped;
                this[key].pct = percent$1(this[key].covered, this[key].total);
            }
        });

        return this;
    }

    /**
     * returns a POJO that is JSON serializable. May be used to get the raw
     * summary object.
     */
    toJSON() {
        return this.data;
    }

    /**
     * return true if summary has no lines of code
     */
    isEmpty() {
        return this.lines.total === 0;
    }
};

dataProperties$1(CoverageSummary$3, [
    'lines',
    'statements',
    'functions',
    'branches',
    'branchesTrue'
]);

var coverageSummary = {
    CoverageSummary: CoverageSummary$3
};

/*
 Copyright 2012-2015, Yahoo Inc.
 Copyrights licensed under the New BSD License. See the accompanying LICENSE file for terms.
 */

const percent = percent$2;
const dataProperties = dataProperties$2;
const { CoverageSummary: CoverageSummary$2 } = coverageSummary;

// returns a data object that represents empty coverage
function emptyCoverage(filePath, reportLogic) {
    const cov = {
        path: filePath,
        statementMap: {},
        fnMap: {},
        branchMap: {},
        s: {},
        f: {},
        b: {}
    };
    if (reportLogic) cov.bT = {};
    return cov;
}

// asserts that a data object "looks like" a coverage object
function assertValidObject(obj) {
    const valid =
        obj &&
        obj.path &&
        obj.statementMap &&
        obj.fnMap &&
        obj.branchMap &&
        obj.s &&
        obj.f &&
        obj.b;
    if (!valid) {
        throw new Error(
            'Invalid file coverage object, missing keys, found:' +
                Object.keys(obj).join(',')
        );
    }
}

const keyFromLoc = ({ start, end }) =>
    `${start.line}|${start.column}|${end.line}|${end.column}`;

const mergeProp = (aHits, aMap, bHits, bMap, itemKey = keyFromLoc) => {
    const aItems = {};
    for (const [key, itemHits] of Object.entries(aHits)) {
        const item = aMap[key];
        aItems[itemKey(item)] = [itemHits, item];
    }
    for (const [key, bItemHits] of Object.entries(bHits)) {
        const bItem = bMap[key];
        const k = itemKey(bItem);

        if (aItems[k]) {
            const aPair = aItems[k];
            if (bItemHits.forEach) {
                // should this throw an exception if aPair[0] is not an array?
                bItemHits.forEach((hits, h) => {
                    if (aPair[0][h] !== undefined) aPair[0][h] += hits;
                    else aPair[0][h] = hits;
                });
            } else {
                aPair[0] += bItemHits;
            }
        } else {
            aItems[k] = [bItemHits, bItem];
        }
    }
    const hits = {};
    const map = {};

    Object.values(aItems).forEach(([itemHits, item], i) => {
        hits[i] = itemHits;
        map[i] = item;
    });

    return [hits, map];
};

/**
 * provides a read-only view of coverage for a single file.
 * The deep structure of this object is documented elsewhere. It has the following
 * properties:
 *
 * * `path` - the file path for which coverage is being tracked
 * * `statementMap` - map of statement locations keyed by statement index
 * * `fnMap` - map of function metadata keyed by function index
 * * `branchMap` - map of branch metadata keyed by branch index
 * * `s` - hit counts for statements
 * * `f` - hit count for functions
 * * `b` - hit count for branches
 */
let FileCoverage$2 = class FileCoverage {
    /**
     * @constructor
     * @param {Object|FileCoverage|String} pathOrObj is a string that initializes
     * and empty coverage object with the specified file path or a data object that
     * has all the required properties for a file coverage object.
     */
    constructor(pathOrObj, reportLogic = false) {
        if (!pathOrObj) {
            throw new Error(
                'Coverage must be initialized with a path or an object'
            );
        }
        if (typeof pathOrObj === 'string') {
            this.data = emptyCoverage(pathOrObj, reportLogic);
        } else if (pathOrObj instanceof FileCoverage$2) {
            this.data = pathOrObj.data;
        } else if (typeof pathOrObj === 'object') {
            this.data = pathOrObj;
        } else {
            throw new Error('Invalid argument to coverage constructor');
        }
        assertValidObject(this.data);
    }

    /**
     * returns computed line coverage from statement coverage.
     * This is a map of hits keyed by line number in the source.
     */
    getLineCoverage() {
        const statementMap = this.data.statementMap;
        const statements = this.data.s;
        const lineMap = Object.create(null);

        Object.entries(statements).forEach(([st, count]) => {
            /* istanbul ignore if: is this even possible? */
            if (!statementMap[st]) {
                return;
            }
            const { line } = statementMap[st].start;
            const prevVal = lineMap[line];
            if (prevVal === undefined || prevVal < count) {
                lineMap[line] = count;
            }
        });
        return lineMap;
    }

    /**
     * returns an array of uncovered line numbers.
     * @returns {Array} an array of line numbers for which no hits have been
     *  collected.
     */
    getUncoveredLines() {
        const lc = this.getLineCoverage();
        const ret = [];
        Object.entries(lc).forEach(([l, hits]) => {
            if (hits === 0) {
                ret.push(l);
            }
        });
        return ret;
    }

    /**
     * returns a map of branch coverage by source line number.
     * @returns {Object} an object keyed by line number. Each object
     * has a `covered`, `total` and `coverage` (percentage) property.
     */
    getBranchCoverageByLine() {
        const branchMap = this.branchMap;
        const branches = this.b;
        const ret = {};
        Object.entries(branchMap).forEach(([k, map]) => {
            const line = map.line || map.loc.start.line;
            const branchData = branches[k];
            ret[line] = ret[line] || [];
            ret[line].push(...branchData);
        });
        Object.entries(ret).forEach(([k, dataArray]) => {
            const covered = dataArray.filter(item => item > 0);
            const coverage = (covered.length / dataArray.length) * 100;
            ret[k] = {
                covered: covered.length,
                total: dataArray.length,
                coverage
            };
        });
        return ret;
    }

    /**
     * return a JSON-serializable POJO for this file coverage object
     */
    toJSON() {
        return this.data;
    }

    /**
     * merges a second coverage object into this one, updating hit counts
     * @param {FileCoverage} other - the coverage object to be merged into this one.
     *  Note that the other object should have the same structure as this one (same file).
     */
    merge(other) {
        if (other.all === true) {
            return;
        }

        if (this.all === true) {
            this.data = other.data;
            return;
        }

        let [hits, map] = mergeProp(
            this.s,
            this.statementMap,
            other.s,
            other.statementMap
        );
        this.data.s = hits;
        this.data.statementMap = map;

        const keyFromLocProp = x => keyFromLoc(x.loc);
        const keyFromLocationsProp = x => keyFromLoc(x.locations[0]);

        [hits, map] = mergeProp(
            this.f,
            this.fnMap,
            other.f,
            other.fnMap,
            keyFromLocProp
        );
        this.data.f = hits;
        this.data.fnMap = map;

        [hits, map] = mergeProp(
            this.b,
            this.branchMap,
            other.b,
            other.branchMap,
            keyFromLocationsProp
        );
        this.data.b = hits;
        this.data.branchMap = map;

        // Tracking additional information about branch truthiness
        // can be optionally enabled:
        if (this.bT && other.bT) {
            [hits, map] = mergeProp(
                this.bT,
                this.branchMap,
                other.bT,
                other.branchMap,
                keyFromLocationsProp
            );
            this.data.bT = hits;
        }
    }

    computeSimpleTotals(property) {
        let stats = this[property];

        if (typeof stats === 'function') {
            stats = stats.call(this);
        }

        const ret = {
            total: Object.keys(stats).length,
            covered: Object.values(stats).filter(v => !!v).length,
            skipped: 0
        };
        ret.pct = percent(ret.covered, ret.total);
        return ret;
    }

    computeBranchTotals(property) {
        const stats = this[property];
        const ret = { total: 0, covered: 0, skipped: 0 };

        Object.values(stats).forEach(branches => {
            ret.covered += branches.filter(hits => hits > 0).length;
            ret.total += branches.length;
        });
        ret.pct = percent(ret.covered, ret.total);
        return ret;
    }

    /**
     * resets hit counts for all statements, functions and branches
     * in this coverage object resulting in zero coverage.
     */
    resetHits() {
        const statements = this.s;
        const functions = this.f;
        const branches = this.b;
        const branchesTrue = this.bT;
        Object.keys(statements).forEach(s => {
            statements[s] = 0;
        });
        Object.keys(functions).forEach(f => {
            functions[f] = 0;
        });
        Object.keys(branches).forEach(b => {
            branches[b].fill(0);
        });
        // Tracking additional information about branch truthiness
        // can be optionally enabled:
        if (branchesTrue) {
            Object.keys(branchesTrue).forEach(bT => {
                branchesTrue[bT].fill(0);
            });
        }
    }

    /**
     * returns a CoverageSummary for this file coverage object
     * @returns {CoverageSummary}
     */
    toSummary() {
        const ret = {};
        ret.lines = this.computeSimpleTotals('getLineCoverage');
        ret.functions = this.computeSimpleTotals('f', 'fnMap');
        ret.statements = this.computeSimpleTotals('s', 'statementMap');
        ret.branches = this.computeBranchTotals('b');
        // Tracking additional information about branch truthiness
        // can be optionally enabled:
        if (this['bt']) {
            ret.branchesTrue = this.computeBranchTotals('bT');
        }
        return new CoverageSummary$2(ret);
    }
};

// expose coverage data attributes
dataProperties(FileCoverage$2, [
    'path',
    'statementMap',
    'fnMap',
    'branchMap',
    's',
    'f',
    'b',
    'bT',
    'all'
]);

var fileCoverage = {
    FileCoverage: FileCoverage$2
};

/*
 Copyright 2012-2015, Yahoo Inc.
 Copyrights licensed under the New BSD License. See the accompanying LICENSE file for terms.
 */

const { FileCoverage: FileCoverage$1 } = fileCoverage;
const { CoverageSummary: CoverageSummary$1 } = coverageSummary;

function maybeConstruct(obj, klass) {
    if (obj instanceof klass) {
        return obj;
    }

    return new klass(obj);
}

function loadMap(source) {
    const data = Object.create(null);
    if (!source) {
        return data;
    }

    Object.entries(source).forEach(([k, cov]) => {
        data[k] = maybeConstruct(cov, FileCoverage$1);
    });

    return data;
}

/** CoverageMap is a map of `FileCoverage` objects keyed by file paths. */
let CoverageMap$1 = class CoverageMap {
    /**
     * @constructor
     * @param {Object} [obj=undefined] obj A coverage map from which to initialize this
     * map's contents. This can be the raw global coverage object.
     */
    constructor(obj) {
        if (obj instanceof CoverageMap$1) {
            this.data = obj.data;
        } else {
            this.data = loadMap(obj);
        }
    }

    /**
     * merges a second coverage map into this one
     * @param {CoverageMap} obj - a CoverageMap or its raw data. Coverage is merged
     *  correctly for the same files and additional file coverage keys are created
     *  as needed.
     */
    merge(obj) {
        const other = maybeConstruct(obj, CoverageMap$1);
        Object.values(other.data).forEach(fc => {
            this.addFileCoverage(fc);
        });
    }

    /**
     * filter the coveragemap based on the callback provided
     * @param {Function (filename)} callback - Returns true if the path
     *  should be included in the coveragemap. False if it should be
     *  removed.
     */
    filter(callback) {
        Object.keys(this.data).forEach(k => {
            if (!callback(k)) {
                delete this.data[k];
            }
        });
    }

    /**
     * returns a JSON-serializable POJO for this coverage map
     * @returns {Object}
     */
    toJSON() {
        return this.data;
    }

    /**
     * returns an array for file paths for which this map has coverage
     * @returns {Array{string}} - array of files
     */
    files() {
        return Object.keys(this.data);
    }

    /**
     * returns the file coverage for the specified file.
     * @param {String} file
     * @returns {FileCoverage}
     */
    fileCoverageFor(file) {
        const fc = this.data[file];
        if (!fc) {
            throw new Error(`No file coverage available for: ${file}`);
        }
        return fc;
    }

    /**
     * adds a file coverage object to this map. If the path for the object,
     * already exists in the map, it is merged with the existing coverage
     * otherwise a new key is added to the map.
     * @param {FileCoverage} fc the file coverage to add
     */
    addFileCoverage(fc) {
        const cov = new FileCoverage$1(fc);
        const { path } = cov;
        if (this.data[path]) {
            this.data[path].merge(cov);
        } else {
            this.data[path] = cov;
        }
    }

    /**
     * returns the coverage summary for all the file coverage objects in this map.
     * @returns {CoverageSummary}
     */
    getCoverageSummary() {
        const ret = new CoverageSummary$1();
        Object.values(this.data).forEach(fc => {
            ret.merge(fc.toSummary());
        });

        return ret;
    }
};

var coverageMap = {
    CoverageMap: CoverageMap$1
};

/*
 Copyright 2012-2015, Yahoo Inc.
 Copyrights licensed under the New BSD License. See the accompanying LICENSE file for terms.
 */

/**
 * istanbul-lib-coverage exports an API that allows you to create and manipulate
 * file coverage, coverage maps (a set of file coverage objects) and summary
 * coverage objects. File coverage for the same file can be merged as can
 * entire coverage maps.
 *
 * @module Exports
 */
const { FileCoverage } = fileCoverage;
const { CoverageMap } = coverageMap;
const { CoverageSummary } = coverageSummary;

istanbulLibCoverage.exports = {
    /**
     * creates a coverage summary object
     * @param {Object} obj an argument with the same semantics
     *  as the one passed to the `CoverageSummary` constructor
     * @returns {CoverageSummary}
     */
    createCoverageSummary(obj) {
        if (obj && obj instanceof CoverageSummary) {
            return obj;
        }
        return new CoverageSummary(obj);
    },
    /**
     * creates a CoverageMap object
     * @param {Object} obj optional - an argument with the same semantics
     *  as the one passed to the CoverageMap constructor.
     * @returns {CoverageMap}
     */
    createCoverageMap(obj) {
        if (obj && obj instanceof CoverageMap) {
            return obj;
        }
        return new CoverageMap(obj);
    },
    /**
     * creates a FileCoverage object
     * @param {Object} obj optional - an argument with the same semantics
     *  as the one passed to the FileCoverage constructor.
     * @returns {FileCoverage}
     */
    createFileCoverage(obj) {
        if (obj && obj instanceof FileCoverage) {
            return obj;
        }
        return new FileCoverage(obj);
    }
};

/** classes exported for reuse */
istanbulLibCoverage.exports.classes = {
    /**
     * the file coverage constructor
     */
    FileCoverage
};

var makeDir$1 = {exports: {}};

var semver$1 = {exports: {}};

(function (module, exports) {
	exports = module.exports = SemVer;

	var debug;
	/* istanbul ignore next */
	if (typeof process === 'object' &&
	    process.env &&
	    process.env.NODE_DEBUG &&
	    /\bsemver\b/i.test(process.env.NODE_DEBUG)) {
	  debug = function () {
	    var args = Array.prototype.slice.call(arguments, 0);
	    args.unshift('SEMVER');
	    console.log.apply(console, args);
	  };
	} else {
	  debug = function () {};
	}

	// Note: this is the semver.org version of the spec that it implements
	// Not necessarily the package version of this code.
	exports.SEMVER_SPEC_VERSION = '2.0.0';

	var MAX_LENGTH = 256;
	var MAX_SAFE_INTEGER = Number.MAX_SAFE_INTEGER ||
	  /* istanbul ignore next */ 9007199254740991;

	// Max safe segment length for coercion.
	var MAX_SAFE_COMPONENT_LENGTH = 16;

	// The actual regexps go on exports.re
	var re = exports.re = [];
	var src = exports.src = [];
	var t = exports.tokens = {};
	var R = 0;

	function tok (n) {
	  t[n] = R++;
	}

	// The following Regular Expressions can be used for tokenizing,
	// validating, and parsing SemVer version strings.

	// ## Numeric Identifier
	// A single `0`, or a non-zero digit followed by zero or more digits.

	tok('NUMERICIDENTIFIER');
	src[t.NUMERICIDENTIFIER] = '0|[1-9]\\d*';
	tok('NUMERICIDENTIFIERLOOSE');
	src[t.NUMERICIDENTIFIERLOOSE] = '[0-9]+';

	// ## Non-numeric Identifier
	// Zero or more digits, followed by a letter or hyphen, and then zero or
	// more letters, digits, or hyphens.

	tok('NONNUMERICIDENTIFIER');
	src[t.NONNUMERICIDENTIFIER] = '\\d*[a-zA-Z-][a-zA-Z0-9-]*';

	// ## Main Version
	// Three dot-separated numeric identifiers.

	tok('MAINVERSION');
	src[t.MAINVERSION] = '(' + src[t.NUMERICIDENTIFIER] + ')\\.' +
	                   '(' + src[t.NUMERICIDENTIFIER] + ')\\.' +
	                   '(' + src[t.NUMERICIDENTIFIER] + ')';

	tok('MAINVERSIONLOOSE');
	src[t.MAINVERSIONLOOSE] = '(' + src[t.NUMERICIDENTIFIERLOOSE] + ')\\.' +
	                        '(' + src[t.NUMERICIDENTIFIERLOOSE] + ')\\.' +
	                        '(' + src[t.NUMERICIDENTIFIERLOOSE] + ')';

	// ## Pre-release Version Identifier
	// A numeric identifier, or a non-numeric identifier.

	tok('PRERELEASEIDENTIFIER');
	src[t.PRERELEASEIDENTIFIER] = '(?:' + src[t.NUMERICIDENTIFIER] +
	                            '|' + src[t.NONNUMERICIDENTIFIER] + ')';

	tok('PRERELEASEIDENTIFIERLOOSE');
	src[t.PRERELEASEIDENTIFIERLOOSE] = '(?:' + src[t.NUMERICIDENTIFIERLOOSE] +
	                                 '|' + src[t.NONNUMERICIDENTIFIER] + ')';

	// ## Pre-release Version
	// Hyphen, followed by one or more dot-separated pre-release version
	// identifiers.

	tok('PRERELEASE');
	src[t.PRERELEASE] = '(?:-(' + src[t.PRERELEASEIDENTIFIER] +
	                  '(?:\\.' + src[t.PRERELEASEIDENTIFIER] + ')*))';

	tok('PRERELEASELOOSE');
	src[t.PRERELEASELOOSE] = '(?:-?(' + src[t.PRERELEASEIDENTIFIERLOOSE] +
	                       '(?:\\.' + src[t.PRERELEASEIDENTIFIERLOOSE] + ')*))';

	// ## Build Metadata Identifier
	// Any combination of digits, letters, or hyphens.

	tok('BUILDIDENTIFIER');
	src[t.BUILDIDENTIFIER] = '[0-9A-Za-z-]+';

	// ## Build Metadata
	// Plus sign, followed by one or more period-separated build metadata
	// identifiers.

	tok('BUILD');
	src[t.BUILD] = '(?:\\+(' + src[t.BUILDIDENTIFIER] +
	             '(?:\\.' + src[t.BUILDIDENTIFIER] + ')*))';

	// ## Full Version String
	// A main version, followed optionally by a pre-release version and
	// build metadata.

	// Note that the only major, minor, patch, and pre-release sections of
	// the version string are capturing groups.  The build metadata is not a
	// capturing group, because it should not ever be used in version
	// comparison.

	tok('FULL');
	tok('FULLPLAIN');
	src[t.FULLPLAIN] = 'v?' + src[t.MAINVERSION] +
	                  src[t.PRERELEASE] + '?' +
	                  src[t.BUILD] + '?';

	src[t.FULL] = '^' + src[t.FULLPLAIN] + '$';

	// like full, but allows v1.2.3 and =1.2.3, which people do sometimes.
	// also, 1.0.0alpha1 (prerelease without the hyphen) which is pretty
	// common in the npm registry.
	tok('LOOSEPLAIN');
	src[t.LOOSEPLAIN] = '[v=\\s]*' + src[t.MAINVERSIONLOOSE] +
	                  src[t.PRERELEASELOOSE] + '?' +
	                  src[t.BUILD] + '?';

	tok('LOOSE');
	src[t.LOOSE] = '^' + src[t.LOOSEPLAIN] + '$';

	tok('GTLT');
	src[t.GTLT] = '((?:<|>)?=?)';

	// Something like "2.*" or "1.2.x".
	// Note that "x.x" is a valid xRange identifer, meaning "any version"
	// Only the first item is strictly required.
	tok('XRANGEIDENTIFIERLOOSE');
	src[t.XRANGEIDENTIFIERLOOSE] = src[t.NUMERICIDENTIFIERLOOSE] + '|x|X|\\*';
	tok('XRANGEIDENTIFIER');
	src[t.XRANGEIDENTIFIER] = src[t.NUMERICIDENTIFIER] + '|x|X|\\*';

	tok('XRANGEPLAIN');
	src[t.XRANGEPLAIN] = '[v=\\s]*(' + src[t.XRANGEIDENTIFIER] + ')' +
	                   '(?:\\.(' + src[t.XRANGEIDENTIFIER] + ')' +
	                   '(?:\\.(' + src[t.XRANGEIDENTIFIER] + ')' +
	                   '(?:' + src[t.PRERELEASE] + ')?' +
	                   src[t.BUILD] + '?' +
	                   ')?)?';

	tok('XRANGEPLAINLOOSE');
	src[t.XRANGEPLAINLOOSE] = '[v=\\s]*(' + src[t.XRANGEIDENTIFIERLOOSE] + ')' +
	                        '(?:\\.(' + src[t.XRANGEIDENTIFIERLOOSE] + ')' +
	                        '(?:\\.(' + src[t.XRANGEIDENTIFIERLOOSE] + ')' +
	                        '(?:' + src[t.PRERELEASELOOSE] + ')?' +
	                        src[t.BUILD] + '?' +
	                        ')?)?';

	tok('XRANGE');
	src[t.XRANGE] = '^' + src[t.GTLT] + '\\s*' + src[t.XRANGEPLAIN] + '$';
	tok('XRANGELOOSE');
	src[t.XRANGELOOSE] = '^' + src[t.GTLT] + '\\s*' + src[t.XRANGEPLAINLOOSE] + '$';

	// Coercion.
	// Extract anything that could conceivably be a part of a valid semver
	tok('COERCE');
	src[t.COERCE] = '(^|[^\\d])' +
	              '(\\d{1,' + MAX_SAFE_COMPONENT_LENGTH + '})' +
	              '(?:\\.(\\d{1,' + MAX_SAFE_COMPONENT_LENGTH + '}))?' +
	              '(?:\\.(\\d{1,' + MAX_SAFE_COMPONENT_LENGTH + '}))?' +
	              '(?:$|[^\\d])';
	tok('COERCERTL');
	re[t.COERCERTL] = new RegExp(src[t.COERCE], 'g');

	// Tilde ranges.
	// Meaning is "reasonably at or greater than"
	tok('LONETILDE');
	src[t.LONETILDE] = '(?:~>?)';

	tok('TILDETRIM');
	src[t.TILDETRIM] = '(\\s*)' + src[t.LONETILDE] + '\\s+';
	re[t.TILDETRIM] = new RegExp(src[t.TILDETRIM], 'g');
	var tildeTrimReplace = '$1~';

	tok('TILDE');
	src[t.TILDE] = '^' + src[t.LONETILDE] + src[t.XRANGEPLAIN] + '$';
	tok('TILDELOOSE');
	src[t.TILDELOOSE] = '^' + src[t.LONETILDE] + src[t.XRANGEPLAINLOOSE] + '$';

	// Caret ranges.
	// Meaning is "at least and backwards compatible with"
	tok('LONECARET');
	src[t.LONECARET] = '(?:\\^)';

	tok('CARETTRIM');
	src[t.CARETTRIM] = '(\\s*)' + src[t.LONECARET] + '\\s+';
	re[t.CARETTRIM] = new RegExp(src[t.CARETTRIM], 'g');
	var caretTrimReplace = '$1^';

	tok('CARET');
	src[t.CARET] = '^' + src[t.LONECARET] + src[t.XRANGEPLAIN] + '$';
	tok('CARETLOOSE');
	src[t.CARETLOOSE] = '^' + src[t.LONECARET] + src[t.XRANGEPLAINLOOSE] + '$';

	// A simple gt/lt/eq thing, or just "" to indicate "any version"
	tok('COMPARATORLOOSE');
	src[t.COMPARATORLOOSE] = '^' + src[t.GTLT] + '\\s*(' + src[t.LOOSEPLAIN] + ')$|^$';
	tok('COMPARATOR');
	src[t.COMPARATOR] = '^' + src[t.GTLT] + '\\s*(' + src[t.FULLPLAIN] + ')$|^$';

	// An expression to strip any whitespace between the gtlt and the thing
	// it modifies, so that `> 1.2.3` ==> `>1.2.3`
	tok('COMPARATORTRIM');
	src[t.COMPARATORTRIM] = '(\\s*)' + src[t.GTLT] +
	                      '\\s*(' + src[t.LOOSEPLAIN] + '|' + src[t.XRANGEPLAIN] + ')';

	// this one has to use the /g flag
	re[t.COMPARATORTRIM] = new RegExp(src[t.COMPARATORTRIM], 'g');
	var comparatorTrimReplace = '$1$2$3';

	// Something like `1.2.3 - 1.2.4`
	// Note that these all use the loose form, because they'll be
	// checked against either the strict or loose comparator form
	// later.
	tok('HYPHENRANGE');
	src[t.HYPHENRANGE] = '^\\s*(' + src[t.XRANGEPLAIN] + ')' +
	                   '\\s+-\\s+' +
	                   '(' + src[t.XRANGEPLAIN] + ')' +
	                   '\\s*$';

	tok('HYPHENRANGELOOSE');
	src[t.HYPHENRANGELOOSE] = '^\\s*(' + src[t.XRANGEPLAINLOOSE] + ')' +
	                        '\\s+-\\s+' +
	                        '(' + src[t.XRANGEPLAINLOOSE] + ')' +
	                        '\\s*$';

	// Star ranges basically just allow anything at all.
	tok('STAR');
	src[t.STAR] = '(<|>)?=?\\s*\\*';

	// Compile to actual regexp objects.
	// All are flag-free, unless they were created above with a flag.
	for (var i = 0; i < R; i++) {
	  debug(i, src[i]);
	  if (!re[i]) {
	    re[i] = new RegExp(src[i]);
	  }
	}

	exports.parse = parse;
	function parse (version, options) {
	  if (!options || typeof options !== 'object') {
	    options = {
	      loose: !!options,
	      includePrerelease: false
	    };
	  }

	  if (version instanceof SemVer) {
	    return version
	  }

	  if (typeof version !== 'string') {
	    return null
	  }

	  if (version.length > MAX_LENGTH) {
	    return null
	  }

	  var r = options.loose ? re[t.LOOSE] : re[t.FULL];
	  if (!r.test(version)) {
	    return null
	  }

	  try {
	    return new SemVer(version, options)
	  } catch (er) {
	    return null
	  }
	}

	exports.valid = valid;
	function valid (version, options) {
	  var v = parse(version, options);
	  return v ? v.version : null
	}

	exports.clean = clean;
	function clean (version, options) {
	  var s = parse(version.trim().replace(/^[=v]+/, ''), options);
	  return s ? s.version : null
	}

	exports.SemVer = SemVer;

	function SemVer (version, options) {
	  if (!options || typeof options !== 'object') {
	    options = {
	      loose: !!options,
	      includePrerelease: false
	    };
	  }
	  if (version instanceof SemVer) {
	    if (version.loose === options.loose) {
	      return version
	    } else {
	      version = version.version;
	    }
	  } else if (typeof version !== 'string') {
	    throw new TypeError('Invalid Version: ' + version)
	  }

	  if (version.length > MAX_LENGTH) {
	    throw new TypeError('version is longer than ' + MAX_LENGTH + ' characters')
	  }

	  if (!(this instanceof SemVer)) {
	    return new SemVer(version, options)
	  }

	  debug('SemVer', version, options);
	  this.options = options;
	  this.loose = !!options.loose;

	  var m = version.trim().match(options.loose ? re[t.LOOSE] : re[t.FULL]);

	  if (!m) {
	    throw new TypeError('Invalid Version: ' + version)
	  }

	  this.raw = version;

	  // these are actually numbers
	  this.major = +m[1];
	  this.minor = +m[2];
	  this.patch = +m[3];

	  if (this.major > MAX_SAFE_INTEGER || this.major < 0) {
	    throw new TypeError('Invalid major version')
	  }

	  if (this.minor > MAX_SAFE_INTEGER || this.minor < 0) {
	    throw new TypeError('Invalid minor version')
	  }

	  if (this.patch > MAX_SAFE_INTEGER || this.patch < 0) {
	    throw new TypeError('Invalid patch version')
	  }

	  // numberify any prerelease numeric ids
	  if (!m[4]) {
	    this.prerelease = [];
	  } else {
	    this.prerelease = m[4].split('.').map(function (id) {
	      if (/^[0-9]+$/.test(id)) {
	        var num = +id;
	        if (num >= 0 && num < MAX_SAFE_INTEGER) {
	          return num
	        }
	      }
	      return id
	    });
	  }

	  this.build = m[5] ? m[5].split('.') : [];
	  this.format();
	}

	SemVer.prototype.format = function () {
	  this.version = this.major + '.' + this.minor + '.' + this.patch;
	  if (this.prerelease.length) {
	    this.version += '-' + this.prerelease.join('.');
	  }
	  return this.version
	};

	SemVer.prototype.toString = function () {
	  return this.version
	};

	SemVer.prototype.compare = function (other) {
	  debug('SemVer.compare', this.version, this.options, other);
	  if (!(other instanceof SemVer)) {
	    other = new SemVer(other, this.options);
	  }

	  return this.compareMain(other) || this.comparePre(other)
	};

	SemVer.prototype.compareMain = function (other) {
	  if (!(other instanceof SemVer)) {
	    other = new SemVer(other, this.options);
	  }

	  return compareIdentifiers(this.major, other.major) ||
	         compareIdentifiers(this.minor, other.minor) ||
	         compareIdentifiers(this.patch, other.patch)
	};

	SemVer.prototype.comparePre = function (other) {
	  if (!(other instanceof SemVer)) {
	    other = new SemVer(other, this.options);
	  }

	  // NOT having a prerelease is > having one
	  if (this.prerelease.length && !other.prerelease.length) {
	    return -1
	  } else if (!this.prerelease.length && other.prerelease.length) {
	    return 1
	  } else if (!this.prerelease.length && !other.prerelease.length) {
	    return 0
	  }

	  var i = 0;
	  do {
	    var a = this.prerelease[i];
	    var b = other.prerelease[i];
	    debug('prerelease compare', i, a, b);
	    if (a === undefined && b === undefined) {
	      return 0
	    } else if (b === undefined) {
	      return 1
	    } else if (a === undefined) {
	      return -1
	    } else if (a === b) {
	      continue
	    } else {
	      return compareIdentifiers(a, b)
	    }
	  } while (++i)
	};

	SemVer.prototype.compareBuild = function (other) {
	  if (!(other instanceof SemVer)) {
	    other = new SemVer(other, this.options);
	  }

	  var i = 0;
	  do {
	    var a = this.build[i];
	    var b = other.build[i];
	    debug('prerelease compare', i, a, b);
	    if (a === undefined && b === undefined) {
	      return 0
	    } else if (b === undefined) {
	      return 1
	    } else if (a === undefined) {
	      return -1
	    } else if (a === b) {
	      continue
	    } else {
	      return compareIdentifiers(a, b)
	    }
	  } while (++i)
	};

	// preminor will bump the version up to the next minor release, and immediately
	// down to pre-release. premajor and prepatch work the same way.
	SemVer.prototype.inc = function (release, identifier) {
	  switch (release) {
	    case 'premajor':
	      this.prerelease.length = 0;
	      this.patch = 0;
	      this.minor = 0;
	      this.major++;
	      this.inc('pre', identifier);
	      break
	    case 'preminor':
	      this.prerelease.length = 0;
	      this.patch = 0;
	      this.minor++;
	      this.inc('pre', identifier);
	      break
	    case 'prepatch':
	      // If this is already a prerelease, it will bump to the next version
	      // drop any prereleases that might already exist, since they are not
	      // relevant at this point.
	      this.prerelease.length = 0;
	      this.inc('patch', identifier);
	      this.inc('pre', identifier);
	      break
	    // If the input is a non-prerelease version, this acts the same as
	    // prepatch.
	    case 'prerelease':
	      if (this.prerelease.length === 0) {
	        this.inc('patch', identifier);
	      }
	      this.inc('pre', identifier);
	      break

	    case 'major':
	      // If this is a pre-major version, bump up to the same major version.
	      // Otherwise increment major.
	      // 1.0.0-5 bumps to 1.0.0
	      // 1.1.0 bumps to 2.0.0
	      if (this.minor !== 0 ||
	          this.patch !== 0 ||
	          this.prerelease.length === 0) {
	        this.major++;
	      }
	      this.minor = 0;
	      this.patch = 0;
	      this.prerelease = [];
	      break
	    case 'minor':
	      // If this is a pre-minor version, bump up to the same minor version.
	      // Otherwise increment minor.
	      // 1.2.0-5 bumps to 1.2.0
	      // 1.2.1 bumps to 1.3.0
	      if (this.patch !== 0 || this.prerelease.length === 0) {
	        this.minor++;
	      }
	      this.patch = 0;
	      this.prerelease = [];
	      break
	    case 'patch':
	      // If this is not a pre-release version, it will increment the patch.
	      // If it is a pre-release it will bump up to the same patch version.
	      // 1.2.0-5 patches to 1.2.0
	      // 1.2.0 patches to 1.2.1
	      if (this.prerelease.length === 0) {
	        this.patch++;
	      }
	      this.prerelease = [];
	      break
	    // This probably shouldn't be used publicly.
	    // 1.0.0 "pre" would become 1.0.0-0 which is the wrong direction.
	    case 'pre':
	      if (this.prerelease.length === 0) {
	        this.prerelease = [0];
	      } else {
	        var i = this.prerelease.length;
	        while (--i >= 0) {
	          if (typeof this.prerelease[i] === 'number') {
	            this.prerelease[i]++;
	            i = -2;
	          }
	        }
	        if (i === -1) {
	          // didn't increment anything
	          this.prerelease.push(0);
	        }
	      }
	      if (identifier) {
	        // 1.2.0-beta.1 bumps to 1.2.0-beta.2,
	        // 1.2.0-beta.fooblz or 1.2.0-beta bumps to 1.2.0-beta.0
	        if (this.prerelease[0] === identifier) {
	          if (isNaN(this.prerelease[1])) {
	            this.prerelease = [identifier, 0];
	          }
	        } else {
	          this.prerelease = [identifier, 0];
	        }
	      }
	      break

	    default:
	      throw new Error('invalid increment argument: ' + release)
	  }
	  this.format();
	  this.raw = this.version;
	  return this
	};

	exports.inc = inc;
	function inc (version, release, loose, identifier) {
	  if (typeof (loose) === 'string') {
	    identifier = loose;
	    loose = undefined;
	  }

	  try {
	    return new SemVer(version, loose).inc(release, identifier).version
	  } catch (er) {
	    return null
	  }
	}

	exports.diff = diff;
	function diff (version1, version2) {
	  if (eq(version1, version2)) {
	    return null
	  } else {
	    var v1 = parse(version1);
	    var v2 = parse(version2);
	    var prefix = '';
	    if (v1.prerelease.length || v2.prerelease.length) {
	      prefix = 'pre';
	      var defaultResult = 'prerelease';
	    }
	    for (var key in v1) {
	      if (key === 'major' || key === 'minor' || key === 'patch') {
	        if (v1[key] !== v2[key]) {
	          return prefix + key
	        }
	      }
	    }
	    return defaultResult // may be undefined
	  }
	}

	exports.compareIdentifiers = compareIdentifiers;

	var numeric = /^[0-9]+$/;
	function compareIdentifiers (a, b) {
	  var anum = numeric.test(a);
	  var bnum = numeric.test(b);

	  if (anum && bnum) {
	    a = +a;
	    b = +b;
	  }

	  return a === b ? 0
	    : (anum && !bnum) ? -1
	    : (bnum && !anum) ? 1
	    : a < b ? -1
	    : 1
	}

	exports.rcompareIdentifiers = rcompareIdentifiers;
	function rcompareIdentifiers (a, b) {
	  return compareIdentifiers(b, a)
	}

	exports.major = major;
	function major (a, loose) {
	  return new SemVer(a, loose).major
	}

	exports.minor = minor;
	function minor (a, loose) {
	  return new SemVer(a, loose).minor
	}

	exports.patch = patch;
	function patch (a, loose) {
	  return new SemVer(a, loose).patch
	}

	exports.compare = compare;
	function compare (a, b, loose) {
	  return new SemVer(a, loose).compare(new SemVer(b, loose))
	}

	exports.compareLoose = compareLoose;
	function compareLoose (a, b) {
	  return compare(a, b, true)
	}

	exports.compareBuild = compareBuild;
	function compareBuild (a, b, loose) {
	  var versionA = new SemVer(a, loose);
	  var versionB = new SemVer(b, loose);
	  return versionA.compare(versionB) || versionA.compareBuild(versionB)
	}

	exports.rcompare = rcompare;
	function rcompare (a, b, loose) {
	  return compare(b, a, loose)
	}

	exports.sort = sort;
	function sort (list, loose) {
	  return list.sort(function (a, b) {
	    return exports.compareBuild(a, b, loose)
	  })
	}

	exports.rsort = rsort;
	function rsort (list, loose) {
	  return list.sort(function (a, b) {
	    return exports.compareBuild(b, a, loose)
	  })
	}

	exports.gt = gt;
	function gt (a, b, loose) {
	  return compare(a, b, loose) > 0
	}

	exports.lt = lt;
	function lt (a, b, loose) {
	  return compare(a, b, loose) < 0
	}

	exports.eq = eq;
	function eq (a, b, loose) {
	  return compare(a, b, loose) === 0
	}

	exports.neq = neq;
	function neq (a, b, loose) {
	  return compare(a, b, loose) !== 0
	}

	exports.gte = gte;
	function gte (a, b, loose) {
	  return compare(a, b, loose) >= 0
	}

	exports.lte = lte;
	function lte (a, b, loose) {
	  return compare(a, b, loose) <= 0
	}

	exports.cmp = cmp;
	function cmp (a, op, b, loose) {
	  switch (op) {
	    case '===':
	      if (typeof a === 'object')
	        a = a.version;
	      if (typeof b === 'object')
	        b = b.version;
	      return a === b

	    case '!==':
	      if (typeof a === 'object')
	        a = a.version;
	      if (typeof b === 'object')
	        b = b.version;
	      return a !== b

	    case '':
	    case '=':
	    case '==':
	      return eq(a, b, loose)

	    case '!=':
	      return neq(a, b, loose)

	    case '>':
	      return gt(a, b, loose)

	    case '>=':
	      return gte(a, b, loose)

	    case '<':
	      return lt(a, b, loose)

	    case '<=':
	      return lte(a, b, loose)

	    default:
	      throw new TypeError('Invalid operator: ' + op)
	  }
	}

	exports.Comparator = Comparator;
	function Comparator (comp, options) {
	  if (!options || typeof options !== 'object') {
	    options = {
	      loose: !!options,
	      includePrerelease: false
	    };
	  }

	  if (comp instanceof Comparator) {
	    if (comp.loose === !!options.loose) {
	      return comp
	    } else {
	      comp = comp.value;
	    }
	  }

	  if (!(this instanceof Comparator)) {
	    return new Comparator(comp, options)
	  }

	  debug('comparator', comp, options);
	  this.options = options;
	  this.loose = !!options.loose;
	  this.parse(comp);

	  if (this.semver === ANY) {
	    this.value = '';
	  } else {
	    this.value = this.operator + this.semver.version;
	  }

	  debug('comp', this);
	}

	var ANY = {};
	Comparator.prototype.parse = function (comp) {
	  var r = this.options.loose ? re[t.COMPARATORLOOSE] : re[t.COMPARATOR];
	  var m = comp.match(r);

	  if (!m) {
	    throw new TypeError('Invalid comparator: ' + comp)
	  }

	  this.operator = m[1] !== undefined ? m[1] : '';
	  if (this.operator === '=') {
	    this.operator = '';
	  }

	  // if it literally is just '>' or '' then allow anything.
	  if (!m[2]) {
	    this.semver = ANY;
	  } else {
	    this.semver = new SemVer(m[2], this.options.loose);
	  }
	};

	Comparator.prototype.toString = function () {
	  return this.value
	};

	Comparator.prototype.test = function (version) {
	  debug('Comparator.test', version, this.options.loose);

	  if (this.semver === ANY || version === ANY) {
	    return true
	  }

	  if (typeof version === 'string') {
	    try {
	      version = new SemVer(version, this.options);
	    } catch (er) {
	      return false
	    }
	  }

	  return cmp(version, this.operator, this.semver, this.options)
	};

	Comparator.prototype.intersects = function (comp, options) {
	  if (!(comp instanceof Comparator)) {
	    throw new TypeError('a Comparator is required')
	  }

	  if (!options || typeof options !== 'object') {
	    options = {
	      loose: !!options,
	      includePrerelease: false
	    };
	  }

	  var rangeTmp;

	  if (this.operator === '') {
	    if (this.value === '') {
	      return true
	    }
	    rangeTmp = new Range(comp.value, options);
	    return satisfies(this.value, rangeTmp, options)
	  } else if (comp.operator === '') {
	    if (comp.value === '') {
	      return true
	    }
	    rangeTmp = new Range(this.value, options);
	    return satisfies(comp.semver, rangeTmp, options)
	  }

	  var sameDirectionIncreasing =
	    (this.operator === '>=' || this.operator === '>') &&
	    (comp.operator === '>=' || comp.operator === '>');
	  var sameDirectionDecreasing =
	    (this.operator === '<=' || this.operator === '<') &&
	    (comp.operator === '<=' || comp.operator === '<');
	  var sameSemVer = this.semver.version === comp.semver.version;
	  var differentDirectionsInclusive =
	    (this.operator === '>=' || this.operator === '<=') &&
	    (comp.operator === '>=' || comp.operator === '<=');
	  var oppositeDirectionsLessThan =
	    cmp(this.semver, '<', comp.semver, options) &&
	    ((this.operator === '>=' || this.operator === '>') &&
	    (comp.operator === '<=' || comp.operator === '<'));
	  var oppositeDirectionsGreaterThan =
	    cmp(this.semver, '>', comp.semver, options) &&
	    ((this.operator === '<=' || this.operator === '<') &&
	    (comp.operator === '>=' || comp.operator === '>'));

	  return sameDirectionIncreasing || sameDirectionDecreasing ||
	    (sameSemVer && differentDirectionsInclusive) ||
	    oppositeDirectionsLessThan || oppositeDirectionsGreaterThan
	};

	exports.Range = Range;
	function Range (range, options) {
	  if (!options || typeof options !== 'object') {
	    options = {
	      loose: !!options,
	      includePrerelease: false
	    };
	  }

	  if (range instanceof Range) {
	    if (range.loose === !!options.loose &&
	        range.includePrerelease === !!options.includePrerelease) {
	      return range
	    } else {
	      return new Range(range.raw, options)
	    }
	  }

	  if (range instanceof Comparator) {
	    return new Range(range.value, options)
	  }

	  if (!(this instanceof Range)) {
	    return new Range(range, options)
	  }

	  this.options = options;
	  this.loose = !!options.loose;
	  this.includePrerelease = !!options.includePrerelease;

	  // First, split based on boolean or ||
	  this.raw = range;
	  this.set = range.split(/\s*\|\|\s*/).map(function (range) {
	    return this.parseRange(range.trim())
	  }, this).filter(function (c) {
	    // throw out any that are not relevant for whatever reason
	    return c.length
	  });

	  if (!this.set.length) {
	    throw new TypeError('Invalid SemVer Range: ' + range)
	  }

	  this.format();
	}

	Range.prototype.format = function () {
	  this.range = this.set.map(function (comps) {
	    return comps.join(' ').trim()
	  }).join('||').trim();
	  return this.range
	};

	Range.prototype.toString = function () {
	  return this.range
	};

	Range.prototype.parseRange = function (range) {
	  var loose = this.options.loose;
	  range = range.trim();
	  // `1.2.3 - 1.2.4` => `>=1.2.3 <=1.2.4`
	  var hr = loose ? re[t.HYPHENRANGELOOSE] : re[t.HYPHENRANGE];
	  range = range.replace(hr, hyphenReplace);
	  debug('hyphen replace', range);
	  // `> 1.2.3 < 1.2.5` => `>1.2.3 <1.2.5`
	  range = range.replace(re[t.COMPARATORTRIM], comparatorTrimReplace);
	  debug('comparator trim', range, re[t.COMPARATORTRIM]);

	  // `~ 1.2.3` => `~1.2.3`
	  range = range.replace(re[t.TILDETRIM], tildeTrimReplace);

	  // `^ 1.2.3` => `^1.2.3`
	  range = range.replace(re[t.CARETTRIM], caretTrimReplace);

	  // normalize spaces
	  range = range.split(/\s+/).join(' ');

	  // At this point, the range is completely trimmed and
	  // ready to be split into comparators.

	  var compRe = loose ? re[t.COMPARATORLOOSE] : re[t.COMPARATOR];
	  var set = range.split(' ').map(function (comp) {
	    return parseComparator(comp, this.options)
	  }, this).join(' ').split(/\s+/);
	  if (this.options.loose) {
	    // in loose mode, throw out any that are not valid comparators
	    set = set.filter(function (comp) {
	      return !!comp.match(compRe)
	    });
	  }
	  set = set.map(function (comp) {
	    return new Comparator(comp, this.options)
	  }, this);

	  return set
	};

	Range.prototype.intersects = function (range, options) {
	  if (!(range instanceof Range)) {
	    throw new TypeError('a Range is required')
	  }

	  return this.set.some(function (thisComparators) {
	    return (
	      isSatisfiable(thisComparators, options) &&
	      range.set.some(function (rangeComparators) {
	        return (
	          isSatisfiable(rangeComparators, options) &&
	          thisComparators.every(function (thisComparator) {
	            return rangeComparators.every(function (rangeComparator) {
	              return thisComparator.intersects(rangeComparator, options)
	            })
	          })
	        )
	      })
	    )
	  })
	};

	// take a set of comparators and determine whether there
	// exists a version which can satisfy it
	function isSatisfiable (comparators, options) {
	  var result = true;
	  var remainingComparators = comparators.slice();
	  var testComparator = remainingComparators.pop();

	  while (result && remainingComparators.length) {
	    result = remainingComparators.every(function (otherComparator) {
	      return testComparator.intersects(otherComparator, options)
	    });

	    testComparator = remainingComparators.pop();
	  }

	  return result
	}

	// Mostly just for testing and legacy API reasons
	exports.toComparators = toComparators;
	function toComparators (range, options) {
	  return new Range(range, options).set.map(function (comp) {
	    return comp.map(function (c) {
	      return c.value
	    }).join(' ').trim().split(' ')
	  })
	}

	// comprised of xranges, tildes, stars, and gtlt's at this point.
	// already replaced the hyphen ranges
	// turn into a set of JUST comparators.
	function parseComparator (comp, options) {
	  debug('comp', comp, options);
	  comp = replaceCarets(comp, options);
	  debug('caret', comp);
	  comp = replaceTildes(comp, options);
	  debug('tildes', comp);
	  comp = replaceXRanges(comp, options);
	  debug('xrange', comp);
	  comp = replaceStars(comp, options);
	  debug('stars', comp);
	  return comp
	}

	function isX (id) {
	  return !id || id.toLowerCase() === 'x' || id === '*'
	}

	// ~, ~> --> * (any, kinda silly)
	// ~2, ~2.x, ~2.x.x, ~>2, ~>2.x ~>2.x.x --> >=2.0.0 <3.0.0
	// ~2.0, ~2.0.x, ~>2.0, ~>2.0.x --> >=2.0.0 <2.1.0
	// ~1.2, ~1.2.x, ~>1.2, ~>1.2.x --> >=1.2.0 <1.3.0
	// ~1.2.3, ~>1.2.3 --> >=1.2.3 <1.3.0
	// ~1.2.0, ~>1.2.0 --> >=1.2.0 <1.3.0
	function replaceTildes (comp, options) {
	  return comp.trim().split(/\s+/).map(function (comp) {
	    return replaceTilde(comp, options)
	  }).join(' ')
	}

	function replaceTilde (comp, options) {
	  var r = options.loose ? re[t.TILDELOOSE] : re[t.TILDE];
	  return comp.replace(r, function (_, M, m, p, pr) {
	    debug('tilde', comp, _, M, m, p, pr);
	    var ret;

	    if (isX(M)) {
	      ret = '';
	    } else if (isX(m)) {
	      ret = '>=' + M + '.0.0 <' + (+M + 1) + '.0.0';
	    } else if (isX(p)) {
	      // ~1.2 == >=1.2.0 <1.3.0
	      ret = '>=' + M + '.' + m + '.0 <' + M + '.' + (+m + 1) + '.0';
	    } else if (pr) {
	      debug('replaceTilde pr', pr);
	      ret = '>=' + M + '.' + m + '.' + p + '-' + pr +
	            ' <' + M + '.' + (+m + 1) + '.0';
	    } else {
	      // ~1.2.3 == >=1.2.3 <1.3.0
	      ret = '>=' + M + '.' + m + '.' + p +
	            ' <' + M + '.' + (+m + 1) + '.0';
	    }

	    debug('tilde return', ret);
	    return ret
	  })
	}

	// ^ --> * (any, kinda silly)
	// ^2, ^2.x, ^2.x.x --> >=2.0.0 <3.0.0
	// ^2.0, ^2.0.x --> >=2.0.0 <3.0.0
	// ^1.2, ^1.2.x --> >=1.2.0 <2.0.0
	// ^1.2.3 --> >=1.2.3 <2.0.0
	// ^1.2.0 --> >=1.2.0 <2.0.0
	function replaceCarets (comp, options) {
	  return comp.trim().split(/\s+/).map(function (comp) {
	    return replaceCaret(comp, options)
	  }).join(' ')
	}

	function replaceCaret (comp, options) {
	  debug('caret', comp, options);
	  var r = options.loose ? re[t.CARETLOOSE] : re[t.CARET];
	  return comp.replace(r, function (_, M, m, p, pr) {
	    debug('caret', comp, _, M, m, p, pr);
	    var ret;

	    if (isX(M)) {
	      ret = '';
	    } else if (isX(m)) {
	      ret = '>=' + M + '.0.0 <' + (+M + 1) + '.0.0';
	    } else if (isX(p)) {
	      if (M === '0') {
	        ret = '>=' + M + '.' + m + '.0 <' + M + '.' + (+m + 1) + '.0';
	      } else {
	        ret = '>=' + M + '.' + m + '.0 <' + (+M + 1) + '.0.0';
	      }
	    } else if (pr) {
	      debug('replaceCaret pr', pr);
	      if (M === '0') {
	        if (m === '0') {
	          ret = '>=' + M + '.' + m + '.' + p + '-' + pr +
	                ' <' + M + '.' + m + '.' + (+p + 1);
	        } else {
	          ret = '>=' + M + '.' + m + '.' + p + '-' + pr +
	                ' <' + M + '.' + (+m + 1) + '.0';
	        }
	      } else {
	        ret = '>=' + M + '.' + m + '.' + p + '-' + pr +
	              ' <' + (+M + 1) + '.0.0';
	      }
	    } else {
	      debug('no pr');
	      if (M === '0') {
	        if (m === '0') {
	          ret = '>=' + M + '.' + m + '.' + p +
	                ' <' + M + '.' + m + '.' + (+p + 1);
	        } else {
	          ret = '>=' + M + '.' + m + '.' + p +
	                ' <' + M + '.' + (+m + 1) + '.0';
	        }
	      } else {
	        ret = '>=' + M + '.' + m + '.' + p +
	              ' <' + (+M + 1) + '.0.0';
	      }
	    }

	    debug('caret return', ret);
	    return ret
	  })
	}

	function replaceXRanges (comp, options) {
	  debug('replaceXRanges', comp, options);
	  return comp.split(/\s+/).map(function (comp) {
	    return replaceXRange(comp, options)
	  }).join(' ')
	}

	function replaceXRange (comp, options) {
	  comp = comp.trim();
	  var r = options.loose ? re[t.XRANGELOOSE] : re[t.XRANGE];
	  return comp.replace(r, function (ret, gtlt, M, m, p, pr) {
	    debug('xRange', comp, ret, gtlt, M, m, p, pr);
	    var xM = isX(M);
	    var xm = xM || isX(m);
	    var xp = xm || isX(p);
	    var anyX = xp;

	    if (gtlt === '=' && anyX) {
	      gtlt = '';
	    }

	    // if we're including prereleases in the match, then we need
	    // to fix this to -0, the lowest possible prerelease value
	    pr = options.includePrerelease ? '-0' : '';

	    if (xM) {
	      if (gtlt === '>' || gtlt === '<') {
	        // nothing is allowed
	        ret = '<0.0.0-0';
	      } else {
	        // nothing is forbidden
	        ret = '*';
	      }
	    } else if (gtlt && anyX) {
	      // we know patch is an x, because we have any x at all.
	      // replace X with 0
	      if (xm) {
	        m = 0;
	      }
	      p = 0;

	      if (gtlt === '>') {
	        // >1 => >=2.0.0
	        // >1.2 => >=1.3.0
	        // >1.2.3 => >= 1.2.4
	        gtlt = '>=';
	        if (xm) {
	          M = +M + 1;
	          m = 0;
	          p = 0;
	        } else {
	          m = +m + 1;
	          p = 0;
	        }
	      } else if (gtlt === '<=') {
	        // <=0.7.x is actually <0.8.0, since any 0.7.x should
	        // pass.  Similarly, <=7.x is actually <8.0.0, etc.
	        gtlt = '<';
	        if (xm) {
	          M = +M + 1;
	        } else {
	          m = +m + 1;
	        }
	      }

	      ret = gtlt + M + '.' + m + '.' + p + pr;
	    } else if (xm) {
	      ret = '>=' + M + '.0.0' + pr + ' <' + (+M + 1) + '.0.0' + pr;
	    } else if (xp) {
	      ret = '>=' + M + '.' + m + '.0' + pr +
	        ' <' + M + '.' + (+m + 1) + '.0' + pr;
	    }

	    debug('xRange return', ret);

	    return ret
	  })
	}

	// Because * is AND-ed with everything else in the comparator,
	// and '' means "any version", just remove the *s entirely.
	function replaceStars (comp, options) {
	  debug('replaceStars', comp, options);
	  // Looseness is ignored here.  star is always as loose as it gets!
	  return comp.trim().replace(re[t.STAR], '')
	}

	// This function is passed to string.replace(re[t.HYPHENRANGE])
	// M, m, patch, prerelease, build
	// 1.2 - 3.4.5 => >=1.2.0 <=3.4.5
	// 1.2.3 - 3.4 => >=1.2.0 <3.5.0 Any 3.4.x will do
	// 1.2 - 3.4 => >=1.2.0 <3.5.0
	function hyphenReplace ($0,
	  from, fM, fm, fp, fpr, fb,
	  to, tM, tm, tp, tpr, tb) {
	  if (isX(fM)) {
	    from = '';
	  } else if (isX(fm)) {
	    from = '>=' + fM + '.0.0';
	  } else if (isX(fp)) {
	    from = '>=' + fM + '.' + fm + '.0';
	  } else {
	    from = '>=' + from;
	  }

	  if (isX(tM)) {
	    to = '';
	  } else if (isX(tm)) {
	    to = '<' + (+tM + 1) + '.0.0';
	  } else if (isX(tp)) {
	    to = '<' + tM + '.' + (+tm + 1) + '.0';
	  } else if (tpr) {
	    to = '<=' + tM + '.' + tm + '.' + tp + '-' + tpr;
	  } else {
	    to = '<=' + to;
	  }

	  return (from + ' ' + to).trim()
	}

	// if ANY of the sets match ALL of its comparators, then pass
	Range.prototype.test = function (version) {
	  if (!version) {
	    return false
	  }

	  if (typeof version === 'string') {
	    try {
	      version = new SemVer(version, this.options);
	    } catch (er) {
	      return false
	    }
	  }

	  for (var i = 0; i < this.set.length; i++) {
	    if (testSet(this.set[i], version, this.options)) {
	      return true
	    }
	  }
	  return false
	};

	function testSet (set, version, options) {
	  for (var i = 0; i < set.length; i++) {
	    if (!set[i].test(version)) {
	      return false
	    }
	  }

	  if (version.prerelease.length && !options.includePrerelease) {
	    // Find the set of versions that are allowed to have prereleases
	    // For example, ^1.2.3-pr.1 desugars to >=1.2.3-pr.1 <2.0.0
	    // That should allow `1.2.3-pr.2` to pass.
	    // However, `1.2.4-alpha.notready` should NOT be allowed,
	    // even though it's within the range set by the comparators.
	    for (i = 0; i < set.length; i++) {
	      debug(set[i].semver);
	      if (set[i].semver === ANY) {
	        continue
	      }

	      if (set[i].semver.prerelease.length > 0) {
	        var allowed = set[i].semver;
	        if (allowed.major === version.major &&
	            allowed.minor === version.minor &&
	            allowed.patch === version.patch) {
	          return true
	        }
	      }
	    }

	    // Version has a -pre, but it's not one of the ones we like.
	    return false
	  }

	  return true
	}

	exports.satisfies = satisfies;
	function satisfies (version, range, options) {
	  try {
	    range = new Range(range, options);
	  } catch (er) {
	    return false
	  }
	  return range.test(version)
	}

	exports.maxSatisfying = maxSatisfying;
	function maxSatisfying (versions, range, options) {
	  var max = null;
	  var maxSV = null;
	  try {
	    var rangeObj = new Range(range, options);
	  } catch (er) {
	    return null
	  }
	  versions.forEach(function (v) {
	    if (rangeObj.test(v)) {
	      // satisfies(v, range, options)
	      if (!max || maxSV.compare(v) === -1) {
	        // compare(max, v, true)
	        max = v;
	        maxSV = new SemVer(max, options);
	      }
	    }
	  });
	  return max
	}

	exports.minSatisfying = minSatisfying;
	function minSatisfying (versions, range, options) {
	  var min = null;
	  var minSV = null;
	  try {
	    var rangeObj = new Range(range, options);
	  } catch (er) {
	    return null
	  }
	  versions.forEach(function (v) {
	    if (rangeObj.test(v)) {
	      // satisfies(v, range, options)
	      if (!min || minSV.compare(v) === 1) {
	        // compare(min, v, true)
	        min = v;
	        minSV = new SemVer(min, options);
	      }
	    }
	  });
	  return min
	}

	exports.minVersion = minVersion;
	function minVersion (range, loose) {
	  range = new Range(range, loose);

	  var minver = new SemVer('0.0.0');
	  if (range.test(minver)) {
	    return minver
	  }

	  minver = new SemVer('0.0.0-0');
	  if (range.test(minver)) {
	    return minver
	  }

	  minver = null;
	  for (var i = 0; i < range.set.length; ++i) {
	    var comparators = range.set[i];

	    comparators.forEach(function (comparator) {
	      // Clone to avoid manipulating the comparator's semver object.
	      var compver = new SemVer(comparator.semver.version);
	      switch (comparator.operator) {
	        case '>':
	          if (compver.prerelease.length === 0) {
	            compver.patch++;
	          } else {
	            compver.prerelease.push(0);
	          }
	          compver.raw = compver.format();
	          /* fallthrough */
	        case '':
	        case '>=':
	          if (!minver || gt(minver, compver)) {
	            minver = compver;
	          }
	          break
	        case '<':
	        case '<=':
	          /* Ignore maximum versions */
	          break
	        /* istanbul ignore next */
	        default:
	          throw new Error('Unexpected operation: ' + comparator.operator)
	      }
	    });
	  }

	  if (minver && range.test(minver)) {
	    return minver
	  }

	  return null
	}

	exports.validRange = validRange;
	function validRange (range, options) {
	  try {
	    // Return '*' instead of '' so that truthiness works.
	    // This will throw if it's invalid anyway
	    return new Range(range, options).range || '*'
	  } catch (er) {
	    return null
	  }
	}

	// Determine if version is less than all the versions possible in the range
	exports.ltr = ltr;
	function ltr (version, range, options) {
	  return outside(version, range, '<', options)
	}

	// Determine if version is greater than all the versions possible in the range.
	exports.gtr = gtr;
	function gtr (version, range, options) {
	  return outside(version, range, '>', options)
	}

	exports.outside = outside;
	function outside (version, range, hilo, options) {
	  version = new SemVer(version, options);
	  range = new Range(range, options);

	  var gtfn, ltefn, ltfn, comp, ecomp;
	  switch (hilo) {
	    case '>':
	      gtfn = gt;
	      ltefn = lte;
	      ltfn = lt;
	      comp = '>';
	      ecomp = '>=';
	      break
	    case '<':
	      gtfn = lt;
	      ltefn = gte;
	      ltfn = gt;
	      comp = '<';
	      ecomp = '<=';
	      break
	    default:
	      throw new TypeError('Must provide a hilo val of "<" or ">"')
	  }

	  // If it satisifes the range it is not outside
	  if (satisfies(version, range, options)) {
	    return false
	  }

	  // From now on, variable terms are as if we're in "gtr" mode.
	  // but note that everything is flipped for the "ltr" function.

	  for (var i = 0; i < range.set.length; ++i) {
	    var comparators = range.set[i];

	    var high = null;
	    var low = null;

	    comparators.forEach(function (comparator) {
	      if (comparator.semver === ANY) {
	        comparator = new Comparator('>=0.0.0');
	      }
	      high = high || comparator;
	      low = low || comparator;
	      if (gtfn(comparator.semver, high.semver, options)) {
	        high = comparator;
	      } else if (ltfn(comparator.semver, low.semver, options)) {
	        low = comparator;
	      }
	    });

	    // If the edge version comparator has a operator then our version
	    // isn't outside it
	    if (high.operator === comp || high.operator === ecomp) {
	      return false
	    }

	    // If the lowest version comparator has an operator and our version
	    // is less than it then it isn't higher than the range
	    if ((!low.operator || low.operator === comp) &&
	        ltefn(version, low.semver)) {
	      return false
	    } else if (low.operator === ecomp && ltfn(version, low.semver)) {
	      return false
	    }
	  }
	  return true
	}

	exports.prerelease = prerelease;
	function prerelease (version, options) {
	  var parsed = parse(version, options);
	  return (parsed && parsed.prerelease.length) ? parsed.prerelease : null
	}

	exports.intersects = intersects;
	function intersects (r1, r2, options) {
	  r1 = new Range(r1, options);
	  r2 = new Range(r2, options);
	  return r1.intersects(r2)
	}

	exports.coerce = coerce;
	function coerce (version, options) {
	  if (version instanceof SemVer) {
	    return version
	  }

	  if (typeof version === 'number') {
	    version = String(version);
	  }

	  if (typeof version !== 'string') {
	    return null
	  }

	  options = options || {};

	  var match = null;
	  if (!options.rtl) {
	    match = version.match(re[t.COERCE]);
	  } else {
	    // Find the right-most coercible string that does not share
	    // a terminus with a more left-ward coercible string.
	    // Eg, '1.2.3.4' wants to coerce '2.3.4', not '3.4' or '4'
	    //
	    // Walk through the string checking with a /g regexp
	    // Manually set the index so as to pick up overlapping matches.
	    // Stop when we get a match that ends at the string end, since no
	    // coercible string can be more right-ward without the same terminus.
	    var next;
	    while ((next = re[t.COERCERTL].exec(version)) &&
	      (!match || match.index + match[0].length !== version.length)
	    ) {
	      if (!match ||
	          next.index + next[0].length !== match.index + match[0].length) {
	        match = next;
	      }
	      re[t.COERCERTL].lastIndex = next.index + next[1].length + next[2].length;
	    }
	    // leave it in a clean state
	    re[t.COERCERTL].lastIndex = -1;
	  }

	  if (match === null) {
	    return null
	  }

	  return parse(match[2] +
	    '.' + (match[3] || '0') +
	    '.' + (match[4] || '0'), options)
	}
} (semver$1, semver$1.exports));

const fs$2 = require$$0$1;
const path$2 = require$$0;
const {promisify} = require$$2;
const semver = semver$1.exports;

const useNativeRecursiveOption = semver.satisfies(process.version, '>=10.12.0');

// https://github.com/nodejs/node/issues/8987
// https://github.com/libuv/libuv/pull/1088
const checkPath = pth => {
	if (process.platform === 'win32') {
		const pathHasInvalidWinCharacters = /[<>:"|?*]/.test(pth.replace(path$2.parse(pth).root, ''));

		if (pathHasInvalidWinCharacters) {
			const error = new Error(`Path contains invalid characters: ${pth}`);
			error.code = 'EINVAL';
			throw error;
		}
	}
};

const processOptions = options => {
	// https://github.com/sindresorhus/make-dir/issues/18
	const defaults = {
		mode: 0o777,
		fs: fs$2
	};

	return {
		...defaults,
		...options
	};
};

const permissionError = pth => {
	// This replicates the exception of `fs.mkdir` with native the
	// `recusive` option when run on an invalid drive under Windows.
	const error = new Error(`operation not permitted, mkdir '${pth}'`);
	error.code = 'EPERM';
	error.errno = -4048;
	error.path = pth;
	error.syscall = 'mkdir';
	return error;
};

const makeDir = async (input, options) => {
	checkPath(input);
	options = processOptions(options);

	const mkdir = promisify(options.fs.mkdir);
	const stat = promisify(options.fs.stat);

	if (useNativeRecursiveOption && options.fs.mkdir === fs$2.mkdir) {
		const pth = path$2.resolve(input);

		await mkdir(pth, {
			mode: options.mode,
			recursive: true
		});

		return pth;
	}

	const make = async pth => {
		try {
			await mkdir(pth, options.mode);

			return pth;
		} catch (error) {
			if (error.code === 'EPERM') {
				throw error;
			}

			if (error.code === 'ENOENT') {
				if (path$2.dirname(pth) === pth) {
					throw permissionError(pth);
				}

				if (error.message.includes('null bytes')) {
					throw error;
				}

				await make(path$2.dirname(pth));

				return make(pth);
			}

			try {
				const stats = await stat(pth);
				if (!stats.isDirectory()) {
					throw new Error('The path is not a directory');
				}
			} catch (_) {
				throw error;
			}

			return pth;
		}
	};

	return make(path$2.resolve(input));
};

makeDir$1.exports = makeDir;

makeDir$1.exports.sync = (input, options) => {
	checkPath(input);
	options = processOptions(options);

	if (useNativeRecursiveOption && options.fs.mkdirSync === fs$2.mkdirSync) {
		const pth = path$2.resolve(input);

		fs$2.mkdirSync(pth, {
			mode: options.mode,
			recursive: true
		});

		return pth;
	}

	const make = pth => {
		try {
			options.fs.mkdirSync(pth, options.mode);
		} catch (error) {
			if (error.code === 'EPERM') {
				throw error;
			}

			if (error.code === 'ENOENT') {
				if (path$2.dirname(pth) === pth) {
					throw permissionError(pth);
				}

				if (error.message.includes('null bytes')) {
					throw error;
				}

				make(path$2.dirname(pth));
				return make(pth);
			}

			try {
				if (!options.fs.statSync(pth).isDirectory()) {
					throw new Error('The path is not a directory');
				}
			} catch (_) {
				throw error;
			}
		}

		return pth;
	};

	return make(path$2.resolve(input));
};

var hasFlag$1 = (flag, argv = process.argv) => {
	const prefix = flag.startsWith('-') ? '' : (flag.length === 1 ? '-' : '--');
	const position = argv.indexOf(prefix + flag);
	const terminatorPosition = argv.indexOf('--');
	return position !== -1 && (terminatorPosition === -1 || position < terminatorPosition);
};

const os = require$$0$2;
const tty = require$$1;
const hasFlag = hasFlag$1;

const {env} = process;

let forceColor;
if (hasFlag('no-color') ||
	hasFlag('no-colors') ||
	hasFlag('color=false') ||
	hasFlag('color=never')) {
	forceColor = 0;
} else if (hasFlag('color') ||
	hasFlag('colors') ||
	hasFlag('color=true') ||
	hasFlag('color=always')) {
	forceColor = 1;
}

if ('FORCE_COLOR' in env) {
	if (env.FORCE_COLOR === 'true') {
		forceColor = 1;
	} else if (env.FORCE_COLOR === 'false') {
		forceColor = 0;
	} else {
		forceColor = env.FORCE_COLOR.length === 0 ? 1 : Math.min(parseInt(env.FORCE_COLOR, 10), 3);
	}
}

function translateLevel(level) {
	if (level === 0) {
		return false;
	}

	return {
		level,
		hasBasic: true,
		has256: level >= 2,
		has16m: level >= 3
	};
}

function supportsColor$1(haveStream, streamIsTTY) {
	if (forceColor === 0) {
		return 0;
	}

	if (hasFlag('color=16m') ||
		hasFlag('color=full') ||
		hasFlag('color=truecolor')) {
		return 3;
	}

	if (hasFlag('color=256')) {
		return 2;
	}

	if (haveStream && !streamIsTTY && forceColor === undefined) {
		return 0;
	}

	const min = forceColor || 0;

	if (env.TERM === 'dumb') {
		return min;
	}

	if (process.platform === 'win32') {
		// Windows 10 build 10586 is the first Windows release that supports 256 colors.
		// Windows 10 build 14931 is the first release that supports 16m/TrueColor.
		const osRelease = os.release().split('.');
		if (
			Number(osRelease[0]) >= 10 &&
			Number(osRelease[2]) >= 10586
		) {
			return Number(osRelease[2]) >= 14931 ? 3 : 2;
		}

		return 1;
	}

	if ('CI' in env) {
		if (['TRAVIS', 'CIRCLECI', 'APPVEYOR', 'GITLAB_CI', 'GITHUB_ACTIONS', 'BUILDKITE'].some(sign => sign in env) || env.CI_NAME === 'codeship') {
			return 1;
		}

		return min;
	}

	if ('TEAMCITY_VERSION' in env) {
		return /^(9\.(0*[1-9]\d*)\.|\d{2,}\.)/.test(env.TEAMCITY_VERSION) ? 1 : 0;
	}

	if (env.COLORTERM === 'truecolor') {
		return 3;
	}

	if ('TERM_PROGRAM' in env) {
		const version = parseInt((env.TERM_PROGRAM_VERSION || '').split('.')[0], 10);

		switch (env.TERM_PROGRAM) {
			case 'iTerm.app':
				return version >= 3 ? 3 : 2;
			case 'Apple_Terminal':
				return 2;
			// No default
		}
	}

	if (/-256(color)?$/i.test(env.TERM)) {
		return 2;
	}

	if (/^screen|^xterm|^vt100|^vt220|^rxvt|color|ansi|cygwin|linux/i.test(env.TERM)) {
		return 1;
	}

	if ('COLORTERM' in env) {
		return 1;
	}

	return min;
}

function getSupportLevel(stream) {
	const level = supportsColor$1(stream, stream && stream.isTTY);
	return translateLevel(level);
}

var supportsColor_1 = {
	supportsColor: getSupportLevel,
	stdout: translateLevel(supportsColor$1(true, tty.isatty(1))),
	stderr: translateLevel(supportsColor$1(true, tty.isatty(2)))
};

/*
 Copyright 2012-2015, Yahoo Inc.
 Copyrights licensed under the New BSD License. See the accompanying LICENSE file for terms.
 */
const path$1 = require$$0;
const fs$1 = require$$0$1;
const mkdirp = makeDir$1.exports;
const supportsColor = supportsColor_1;

/**
 * Base class for writing content
 * @class ContentWriter
 * @constructor
 */
class ContentWriter {
    /**
     * returns the colorized version of a string. Typically,
     * content writers that write to files will return the
     * same string and ones writing to a tty will wrap it in
     * appropriate escape sequences.
     * @param {String} str the string to colorize
     * @param {String} clazz one of `high`, `medium` or `low`
     * @returns {String} the colorized form of the string
     */
    colorize(str /*, clazz*/) {
        return str;
    }

    /**
     * writes a string appended with a newline to the destination
     * @param {String} str the string to write
     */
    println(str) {
        this.write(`${str}\n`);
    }

    /**
     * closes this content writer. Should be called after all writes are complete.
     */
    close() {}
}

/**
 * a content writer that writes to a file
 * @param {Number} fd - the file descriptor
 * @extends ContentWriter
 * @constructor
 */
class FileContentWriter extends ContentWriter {
    constructor(fd) {
        super();

        this.fd = fd;
    }

    write(str) {
        fs$1.writeSync(this.fd, str);
    }

    close() {
        fs$1.closeSync(this.fd);
    }
}

// allow stdout to be captured for tests.
let capture = false;
let output = '';

/**
 * a content writer that writes to the console
 * @extends ContentWriter
 * @constructor
 */
class ConsoleWriter extends ContentWriter {
    write(str) {
        if (capture) {
            output += str;
        } else {
            process.stdout.write(str);
        }
    }

    colorize(str, clazz) {
        const colors = {
            low: '31;1',
            medium: '33;1',
            high: '32;1'
        };

        /* istanbul ignore next: different modes for CI and local */
        if (supportsColor.stdout && colors[clazz]) {
            return `\u001b[${colors[clazz]}m${str}\u001b[0m`;
        }
        return str;
    }
}

/**
 * utility for writing files under a specific directory
 * @class FileWriter
 * @param {String} baseDir the base directory under which files should be written
 * @constructor
 */
let FileWriter$1 = class FileWriter {
    constructor(baseDir) {
        if (!baseDir) {
            throw new Error('baseDir must be specified');
        }
        this.baseDir = baseDir;
    }

    /**
     * static helpers for capturing stdout report output;
     * super useful for tests!
     */
    static startCapture() {
        capture = true;
    }

    static stopCapture() {
        capture = false;
    }

    static getOutput() {
        return output;
    }

    static resetOutput() {
        output = '';
    }

    /**
     * returns a FileWriter that is rooted at the supplied subdirectory
     * @param {String} subdir the subdirectory under which to root the
     *  returned FileWriter
     * @returns {FileWriter}
     */
    writerForDir(subdir) {
        if (path$1.isAbsolute(subdir)) {
            throw new Error(
                `Cannot create subdir writer for absolute path: ${subdir}`
            );
        }
        return new FileWriter$1(`${this.baseDir}/${subdir}`);
    }

    /**
     * copies a file from a source directory to a destination name
     * @param {String} source path to source file
     * @param {String} dest relative path to destination file
     * @param {String} [header=undefined] optional text to prepend to destination
     *  (e.g., an "this file is autogenerated" comment, copyright notice, etc.)
     */
    copyFile(source, dest, header) {
        if (path$1.isAbsolute(dest)) {
            throw new Error(`Cannot write to absolute path: ${dest}`);
        }
        dest = path$1.resolve(this.baseDir, dest);
        mkdirp.sync(path$1.dirname(dest));
        let contents;
        if (header) {
            contents = header + fs$1.readFileSync(source, 'utf8');
        } else {
            contents = fs$1.readFileSync(source);
        }
        fs$1.writeFileSync(dest, contents);
    }

    /**
     * returns a content writer for writing content to the supplied file.
     * @param {String|null} file the relative path to the file or the special
     *  values `"-"` or `null` for writing to the console
     * @returns {ContentWriter}
     */
    writeFile(file) {
        if (file === null || file === '-') {
            return new ConsoleWriter();
        }
        if (path$1.isAbsolute(file)) {
            throw new Error(`Cannot write to absolute path: ${file}`);
        }
        file = path$1.resolve(this.baseDir, file);
        mkdirp.sync(path$1.dirname(file));
        return new FileContentWriter(fs$1.openSync(file, 'w'));
    }
};

var fileWriter = FileWriter$1;

/*
 Copyright 2012-2015, Yahoo Inc.
 Copyrights licensed under the New BSD License. See the accompanying LICENSE file for terms.
 */
const INDENT = '  ';

function attrString(attrs) {
    return Object.entries(attrs || {})
        .map(([k, v]) => ` ${k}="${v}"`)
        .join('');
}

/**
 * a utility class to produce well-formed, indented XML
 * @param {ContentWriter} contentWriter the content writer that this utility wraps
 * @constructor
 */
let XMLWriter$1 = class XMLWriter {
    constructor(contentWriter) {
        this.cw = contentWriter;
        this.stack = [];
    }

    indent(str) {
        return this.stack.map(() => INDENT).join('') + str;
    }

    /**
     * writes the opening XML tag with the supplied attributes
     * @param {String} name tag name
     * @param {Object} [attrs=null] attrs attributes for the tag
     */
    openTag(name, attrs) {
        const str = this.indent(`<${name + attrString(attrs)}>`);
        this.cw.println(str);
        this.stack.push(name);
    }

    /**
     * closes an open XML tag.
     * @param {String} name - tag name to close. This must match the writer's
     *  notion of the tag that is currently open.
     */
    closeTag(name) {
        if (this.stack.length === 0) {
            throw new Error(`Attempt to close tag ${name} when not opened`);
        }
        const stashed = this.stack.pop();
        const str = `</${name}>`;

        if (stashed !== name) {
            throw new Error(
                `Attempt to close tag ${name} when ${stashed} was the one open`
            );
        }
        this.cw.println(this.indent(str));
    }

    /**
     * writes a tag and its value opening and closing it at the same time
     * @param {String} name tag name
     * @param {Object} [attrs=null] attrs tag attributes
     * @param {String} [content=null] content optional tag content
     */
    inlineTag(name, attrs, content) {
        let str = '<' + name + attrString(attrs);
        if (content) {
            str += `>${content}</${name}>`;
        } else {
            str += '/>';
        }
        str = this.indent(str);
        this.cw.println(str);
    }

    /**
     * closes all open tags and ends the document
     */
    closeAll() {
        this.stack
            .slice()
            .reverse()
            .forEach(name => {
                this.closeTag(name);
            });
    }
};

var xmlWriter = XMLWriter$1;

/*
 Copyright 2012-2015, Yahoo Inc.
 Copyrights licensed under the New BSD License. See the accompanying LICENSE file for terms.
 */

/**
 * An object with methods that are called during the traversal of the coverage tree.
 * A visitor has the following methods that are called during tree traversal.
 *
 *   * `onStart(root, state)` - called before traversal begins
 *   * `onSummary(node, state)` - called for every summary node
 *   * `onDetail(node, state)` - called for every detail node
 *   * `onSummaryEnd(node, state)` - called after all children have been visited for
 *      a summary node.
 *   * `onEnd(root, state)` - called after traversal ends
 *
 * @param delegate - a partial visitor that only implements the methods of interest
 *  The visitor object supplies the missing methods as noops. For example, reports
 *  that only need the final coverage summary need implement `onStart` and nothing
 *  else. Reports that use only detailed coverage information need implement `onDetail`
 *  and nothing else.
 * @constructor
 */
class Visitor {
    constructor(delegate) {
        this.delegate = delegate;
    }
}

['Start', 'End', 'Summary', 'SummaryEnd', 'Detail']
    .map(k => `on${k}`)
    .forEach(fn => {
        Object.defineProperty(Visitor.prototype, fn, {
            writable: true,
            value(node, state) {
                if (typeof this.delegate[fn] === 'function') {
                    this.delegate[fn](node, state);
                }
            }
        });
    });

class CompositeVisitor extends Visitor {
    constructor(visitors) {
        super();

        if (!Array.isArray(visitors)) {
            visitors = [visitors];
        }
        this.visitors = visitors.map(v => {
            if (v instanceof Visitor) {
                return v;
            }
            return new Visitor(v);
        });
    }
}

['Start', 'Summary', 'SummaryEnd', 'Detail', 'End']
    .map(k => `on${k}`)
    .forEach(fn => {
        Object.defineProperty(CompositeVisitor.prototype, fn, {
            value(node, state) {
                this.visitors.forEach(v => {
                    v[fn](node, state);
                });
            }
        });
    });

let BaseNode$1 = class BaseNode {
    isRoot() {
        return !this.getParent();
    }

    /**
     * visit all nodes depth-first from this node down. Note that `onStart`
     * and `onEnd` are never called on the visitor even if the current
     * node is the root of the tree.
     * @param visitor a full visitor that is called during tree traversal
     * @param state optional state that is passed around
     */
    visit(visitor, state) {
        if (this.isSummary()) {
            visitor.onSummary(this, state);
        } else {
            visitor.onDetail(this, state);
        }

        this.getChildren().forEach(child => {
            child.visit(visitor, state);
        });

        if (this.isSummary()) {
            visitor.onSummaryEnd(this, state);
        }
    }
};

/**
 * abstract base class for a coverage tree.
 * @constructor
 */
let BaseTree$1 = class BaseTree {
    constructor(root) {
        this.root = root;
    }

    /**
     * returns the root node of the tree
     */
    getRoot() {
        return this.root;
    }

    /**
     * visits the tree depth-first with the supplied partial visitor
     * @param visitor - a potentially partial visitor
     * @param state - the state to be passed around during tree traversal
     */
    visit(visitor, state) {
        if (!(visitor instanceof Visitor)) {
            visitor = new Visitor(visitor);
        }
        visitor.onStart(this.getRoot(), state);
        this.getRoot().visit(visitor, state);
        visitor.onEnd(this.getRoot(), state);
    }
};

var tree$1 = {
    BaseTree: BaseTree$1,
    BaseNode: BaseNode$1,
    Visitor,
    CompositeVisitor
};

/*
 Copyright 2012-2015, Yahoo Inc.
 Copyrights licensed under the New BSD License. See the accompanying LICENSE file for terms.
 */
var watermarks$2 = {
    getDefault() {
        return {
            statements: [50, 80],
            functions: [50, 80],
            branches: [50, 80],
            lines: [50, 80]
        };
    }
};

/*
 Copyright 2012-2015, Yahoo Inc.
 Copyrights licensed under the New BSD License. See the accompanying LICENSE file for terms.
 */

const path = require$$0;
let parsePath$1 = path.parse;
let SEP = path.sep;
const origParser = parsePath$1;
const origSep = SEP;

function makeRelativeNormalizedPath(str, sep) {
    const parsed = parsePath$1(str);
    let root = parsed.root;
    let dir;
    let file = parsed.base;
    let quoted;
    let pos;

    // handle a weird windows case separately
    if (sep === '\\') {
        pos = root.indexOf(':\\');
        if (pos >= 0) {
            root = root.substring(0, pos + 2);
        }
    }
    dir = parsed.dir.substring(root.length);

    if (str === '') {
        return [];
    }

    if (sep !== '/') {
        quoted = new RegExp(sep.replace(/\W/g, '\\$&'), 'g');
        dir = dir.replace(quoted, '/');
        file = file.replace(quoted, '/'); // excessively paranoid?
    }

    if (dir !== '') {
        dir = `${dir}/${file}`;
    } else {
        dir = file;
    }
    if (dir.substring(0, 1) === '/') {
        dir = dir.substring(1);
    }
    dir = dir.split(/\/+/);
    return dir;
}

let Path$1 = class Path {
    constructor(strOrArray) {
        if (Array.isArray(strOrArray)) {
            this.v = strOrArray;
        } else if (typeof strOrArray === 'string') {
            this.v = makeRelativeNormalizedPath(strOrArray, SEP);
        } else {
            throw new Error(
                `Invalid Path argument must be string or array:${strOrArray}`
            );
        }
    }

    toString() {
        return this.v.join('/');
    }

    hasParent() {
        return this.v.length > 0;
    }

    parent() {
        if (!this.hasParent()) {
            throw new Error('Unable to get parent for 0 elem path');
        }
        const p = this.v.slice();
        p.pop();
        return new Path$1(p);
    }

    elements() {
        return this.v.slice();
    }

    name() {
        return this.v.slice(-1)[0];
    }

    contains(other) {
        let i;
        if (other.length > this.length) {
            return false;
        }
        for (i = 0; i < other.length; i += 1) {
            if (this.v[i] !== other.v[i]) {
                return false;
            }
        }
        return true;
    }

    ancestorOf(other) {
        return other.contains(this) && other.length !== this.length;
    }

    descendantOf(other) {
        return this.contains(other) && other.length !== this.length;
    }

    commonPrefixPath(other) {
        const len = this.length > other.length ? other.length : this.length;
        let i;
        const ret = [];

        for (i = 0; i < len; i += 1) {
            if (this.v[i] === other.v[i]) {
                ret.push(this.v[i]);
            } else {
                break;
            }
        }
        return new Path$1(ret);
    }

    static compare(a, b) {
        const al = a.length;
        const bl = b.length;

        if (al < bl) {
            return -1;
        }

        if (al > bl) {
            return 1;
        }

        const astr = a.toString();
        const bstr = b.toString();
        return astr < bstr ? -1 : astr > bstr ? 1 : 0;
    }
};

['push', 'pop', 'shift', 'unshift', 'splice'].forEach(fn => {
    Object.defineProperty(Path$1.prototype, fn, {
        value(...args) {
            return this.v[fn](...args);
        }
    });
});

Object.defineProperty(Path$1.prototype, 'length', {
    enumerable: true,
    get() {
        return this.v.length;
    }
});

var path_1 = Path$1;
Path$1.tester = {
    setParserAndSep(p, sep) {
        parsePath$1 = p;
        SEP = sep;
    },
    reset() {
        parsePath$1 = origParser;
        SEP = origSep;
    }
};

/*
 Copyright 2012-2015, Yahoo Inc.
 Copyrights licensed under the New BSD License. See the accompanying LICENSE file for terms.
 */

const coverage = istanbulLibCoverage.exports;
const Path = path_1;
const { BaseNode, BaseTree } = tree$1;

class ReportNode extends BaseNode {
    constructor(path, fileCoverage) {
        super();

        this.path = path;
        this.parent = null;
        this.fileCoverage = fileCoverage;
        this.children = [];
    }

    static createRoot(children) {
        const root = new ReportNode(new Path([]));

        children.forEach(child => {
            root.addChild(child);
        });

        return root;
    }

    addChild(child) {
        child.parent = this;
        this.children.push(child);
    }

    asRelative(p) {
        if (p.substring(0, 1) === '/') {
            return p.substring(1);
        }
        return p;
    }

    getQualifiedName() {
        return this.asRelative(this.path.toString());
    }

    getRelativeName() {
        const parent = this.getParent();
        const myPath = this.path;
        let relPath;
        let i;
        const parentPath = parent ? parent.path : new Path([]);
        if (parentPath.ancestorOf(myPath)) {
            relPath = new Path(myPath.elements());
            for (i = 0; i < parentPath.length; i += 1) {
                relPath.shift();
            }
            return this.asRelative(relPath.toString());
        }
        return this.asRelative(this.path.toString());
    }

    getParent() {
        return this.parent;
    }

    getChildren() {
        return this.children;
    }

    isSummary() {
        return !this.fileCoverage;
    }

    getFileCoverage() {
        return this.fileCoverage;
    }

    getCoverageSummary(filesOnly) {
        const cacheProp = `c_${filesOnly ? 'files' : 'full'}`;
        let summary;

        if (Object.prototype.hasOwnProperty.call(this, cacheProp)) {
            return this[cacheProp];
        }

        if (!this.isSummary()) {
            summary = this.getFileCoverage().toSummary();
        } else {
            let count = 0;
            summary = coverage.createCoverageSummary();
            this.getChildren().forEach(child => {
                if (filesOnly && child.isSummary()) {
                    return;
                }
                count += 1;
                summary.merge(child.getCoverageSummary(filesOnly));
            });
            if (count === 0 && filesOnly) {
                summary = null;
            }
        }
        this[cacheProp] = summary;
        return summary;
    }
}

class ReportTree extends BaseTree {
    constructor(root, childPrefix) {
        super(root);

        const maybePrefix = node => {
            if (childPrefix && !node.isRoot()) {
                node.path.unshift(childPrefix);
            }
        };
        this.visit({
            onDetail: maybePrefix,
            onSummary(node) {
                maybePrefix(node);
                node.children.sort((a, b) => {
                    const astr = a.path.toString();
                    const bstr = b.path.toString();
                    return astr < bstr
                        ? -1
                        : astr > bstr
                        ? 1
                        : /* istanbul ignore next */ 0;
                });
            }
        });
    }
}

function findCommonParent(paths) {
    return paths.reduce(
        (common, path) => common.commonPrefixPath(path),
        paths[0] || new Path([])
    );
}

function findOrCreateParent(parentPath, nodeMap, created = () => {}) {
    let parent = nodeMap[parentPath.toString()];

    if (!parent) {
        parent = new ReportNode(parentPath);
        nodeMap[parentPath.toString()] = parent;
        created(parentPath, parent);
    }

    return parent;
}

function toDirParents(list) {
    const nodeMap = Object.create(null);
    list.forEach(o => {
        const parent = findOrCreateParent(o.path.parent(), nodeMap);
        parent.addChild(new ReportNode(o.path, o.fileCoverage));
    });

    return Object.values(nodeMap);
}

function addAllPaths(topPaths, nodeMap, path, node) {
    const parent = findOrCreateParent(
        path.parent(),
        nodeMap,
        (parentPath, parent) => {
            if (parentPath.hasParent()) {
                addAllPaths(topPaths, nodeMap, parentPath, parent);
            } else {
                topPaths.push(parent);
            }
        }
    );

    parent.addChild(node);
}

function foldIntoOneDir(node, parent) {
    const { children } = node;
    if (children.length === 1 && !children[0].fileCoverage) {
        children[0].parent = parent;
        return foldIntoOneDir(children[0], parent);
    }
    node.children = children.map(child => foldIntoOneDir(child, node));
    return node;
}

function pkgSummaryPrefix(dirParents, commonParent) {
    if (!dirParents.some(dp => dp.path.length === 0)) {
        return;
    }

    if (commonParent.length === 0) {
        return 'root';
    }

    return commonParent.name();
}

let SummarizerFactory$1 = class SummarizerFactory {
    constructor(coverageMap, defaultSummarizer = 'pkg') {
        this._coverageMap = coverageMap;
        this._defaultSummarizer = defaultSummarizer;
        this._initialList = coverageMap.files().map(filePath => ({
            filePath,
            path: new Path(filePath),
            fileCoverage: coverageMap.fileCoverageFor(filePath)
        }));
        this._commonParent = findCommonParent(
            this._initialList.map(o => o.path.parent())
        );
        if (this._commonParent.length > 0) {
            this._initialList.forEach(o => {
                o.path.splice(0, this._commonParent.length);
            });
        }
    }

    get defaultSummarizer() {
        return this[this._defaultSummarizer];
    }

    get flat() {
        if (!this._flat) {
            this._flat = new ReportTree(
                ReportNode.createRoot(
                    this._initialList.map(
                        node => new ReportNode(node.path, node.fileCoverage)
                    )
                )
            );
        }

        return this._flat;
    }

    _createPkg() {
        const dirParents = toDirParents(this._initialList);
        if (dirParents.length === 1) {
            return new ReportTree(dirParents[0]);
        }

        return new ReportTree(
            ReportNode.createRoot(dirParents),
            pkgSummaryPrefix(dirParents, this._commonParent)
        );
    }

    get pkg() {
        if (!this._pkg) {
            this._pkg = this._createPkg();
        }

        return this._pkg;
    }

    _createNested() {
        const nodeMap = Object.create(null);
        const topPaths = [];
        this._initialList.forEach(o => {
            const node = new ReportNode(o.path, o.fileCoverage);
            addAllPaths(topPaths, nodeMap, o.path, node);
        });

        const topNodes = topPaths.map(node => foldIntoOneDir(node));
        if (topNodes.length === 1) {
            return new ReportTree(topNodes[0]);
        }

        return new ReportTree(ReportNode.createRoot(topNodes));
    }

    get nested() {
        if (!this._nested) {
            this._nested = this._createNested();
        }

        return this._nested;
    }
};

var summarizerFactory = SummarizerFactory$1;

/*
 Copyright 2012-2015, Yahoo Inc.
 Copyrights licensed under the New BSD License. See the accompanying LICENSE file for terms.
 */
const fs = require$$0$1;
const FileWriter = fileWriter;
const XMLWriter = xmlWriter;
const tree = tree$1;
const watermarks$1 = watermarks$2;
const SummarizerFactory = summarizerFactory;

function defaultSourceLookup(path) {
    try {
        return fs.readFileSync(path, 'utf8');
    } catch (ex) {
        throw new Error(`Unable to lookup source: ${path} (${ex.message})`);
    }
}

function normalizeWatermarks(specified = {}) {
    Object.entries(watermarks$1.getDefault()).forEach(([k, value]) => {
        const specValue = specified[k];
        if (!Array.isArray(specValue) || specValue.length !== 2) {
            specified[k] = value;
        }
    });

    return specified;
}

/**
 * A reporting context that is passed to report implementations
 * @param {Object} [opts=null] opts options
 * @param {String} [opts.dir='coverage'] opts.dir the reporting directory
 * @param {Object} [opts.watermarks=null] opts.watermarks watermarks for
 *  statements, lines, branches and functions
 * @param {Function} [opts.sourceFinder=fsLookup] opts.sourceFinder a
 *  function that returns source code given a file path. Defaults to
 *  filesystem lookups based on path.
 * @constructor
 */
let Context$1 = class Context {
    constructor(opts) {
        this.dir = opts.dir || 'coverage';
        this.watermarks = normalizeWatermarks(opts.watermarks);
        this.sourceFinder = opts.sourceFinder || defaultSourceLookup;
        this._summarizerFactory = new SummarizerFactory(
            opts.coverageMap,
            opts.defaultSummarizer
        );
        this.data = {};
    }

    /**
     * returns a FileWriter implementation for reporting use. Also available
     * as the `writer` property on the context.
     * @returns {Writer}
     */
    getWriter() {
        return this.writer;
    }

    /**
     * returns the source code for the specified file path or throws if
     * the source could not be found.
     * @param {String} filePath the file path as found in a file coverage object
     * @returns {String} the source code
     */
    getSource(filePath) {
        return this.sourceFinder(filePath);
    }

    /**
     * returns the coverage class given a coverage
     * types and a percentage value.
     * @param {String} type - the coverage type, one of `statements`, `functions`,
     *  `branches`, or `lines`
     * @param {Number} value - the percentage value
     * @returns {String} one of `high`, `medium` or `low`
     */
    classForPercent(type, value) {
        const watermarks = this.watermarks[type];
        if (!watermarks) {
            return 'unknown';
        }
        if (value < watermarks[0]) {
            return 'low';
        }
        if (value >= watermarks[1]) {
            return 'high';
        }
        return 'medium';
    }

    /**
     * returns an XML writer for the supplied content writer
     * @param {ContentWriter} contentWriter the content writer to which the returned XML writer
     *  writes data
     * @returns {XMLWriter}
     */
    getXMLWriter(contentWriter) {
        return new XMLWriter(contentWriter);
    }

    /**
     * returns a full visitor given a partial one.
     * @param {Object} partialVisitor a partial visitor only having the functions of
     *  interest to the caller. These functions are called with a scope that is the
     *  supplied object.
     * @returns {Visitor}
     */
    getVisitor(partialVisitor) {
        return new tree.Visitor(partialVisitor);
    }

    getTree(name = 'defaultSummarizer') {
        return this._summarizerFactory[name];
    }
};

Object.defineProperty(Context$1.prototype, 'writer', {
    enumerable: true,
    get() {
        if (!this.data.writer) {
            this.data.writer = new FileWriter(this.dir);
        }
        return this.data.writer;
    }
});

var context = Context$1;

// TODO: switch to class private field when targetting node.js 12
const _summarizer = Symbol('ReportBase.#summarizer');

let ReportBase$1 = class ReportBase {
    constructor(opts = {}) {
        this[_summarizer] = opts.summarizer;
    }

    execute(context) {
        context.getTree(this[_summarizer]).visit(this, context);
    }
};

var reportBase = ReportBase$1;

/*
 Copyright 2012-2015, Yahoo Inc.
 Copyrights licensed under the New BSD License. See the accompanying LICENSE file for terms.
 */

/**
 * @module Exports
 */

const Context = context;
const watermarks = watermarks$2;
const ReportBase = reportBase;

var istanbulLibReport = {
    /**
     * returns a reporting context for the supplied options
     * @param {Object} [opts=null] opts
     * @returns {Context}
     */
    createContext(opts) {
        return new Context(opts);
    },

    /**
     * returns the default watermarks that would be used when not
     * overridden
     * @returns {Object} an object with `statements`, `functions`, `branches`,
     *  and `line` keys. Each value is a 2 element array that has the low and
     *  high watermark as percentages.
     */
    getDefaultWatermarks() {
        return watermarks.getDefault();
    },

    /**
     * Base class for all reports
     */
    ReportBase
};

function commonjsRequire(path) {
	throw new Error('Could not dynamically require "' + path + '". Please configure the dynamicRequireTargets or/and ignoreDynamicRequires option of @rollup/plugin-commonjs appropriately for this require call to work.');
}

/*
 Copyright 2012-2015, Yahoo Inc.
 Copyrights licensed under the New BSD License. See the accompanying LICENSE file for terms.
 */

var lcovonly;
var hasRequiredLcovonly;

function requireLcovonly () {
	if (hasRequiredLcovonly) return lcovonly;
	hasRequiredLcovonly = 1;
	const { ReportBase } = istanbulLibReport;

	class LcovOnlyReport extends ReportBase {
	    constructor(opts) {
	        super();
	        opts = opts || {};
	        this.file = opts.file || 'lcov.info';
	        this.projectRoot = opts.projectRoot || process.cwd();
	        this.contentWriter = null;
	    }

	    onStart(root, context) {
	        this.contentWriter = context.writer.writeFile(this.file);
	    }

	    onDetail(node) {
	        const fc = node.getFileCoverage();
	        const writer = this.contentWriter;
	        const functions = fc.f;
	        const functionMap = fc.fnMap;
	        const lines = fc.getLineCoverage();
	        const branches = fc.b;
	        const branchMap = fc.branchMap;
	        const summary = node.getCoverageSummary();
	        const path = require$$0;

	        writer.println('TN:');
	        const fileName = path.relative(this.projectRoot, fc.path);
	        writer.println('SF:' + fileName);

	        Object.values(functionMap).forEach(meta => {
	            // Some versions of the instrumenter in the wild populate 'loc'
	            // but not 'decl':
	            const decl = meta.decl || meta.loc;
	            writer.println('FN:' + [decl.start.line, meta.name].join(','));
	        });
	        writer.println('FNF:' + summary.functions.total);
	        writer.println('FNH:' + summary.functions.covered);

	        Object.entries(functionMap).forEach(([key, meta]) => {
	            const stats = functions[key];
	            writer.println('FNDA:' + [stats, meta.name].join(','));
	        });

	        Object.entries(lines).forEach(entry => {
	            writer.println('DA:' + entry.join(','));
	        });
	        writer.println('LF:' + summary.lines.total);
	        writer.println('LH:' + summary.lines.covered);

	        Object.entries(branches).forEach(([key, branchArray]) => {
	            const meta = branchMap[key];
	            if (meta) {
	                const { line } = meta.loc.start;
	                branchArray.forEach((b, i) => {
	                    writer.println('BRDA:' + [line, key, i, b].join(','));
	                });
	            } else {
	                console.warn('Missing coverage entries in', fileName, key);
	            }
	        });
	        writer.println('BRF:' + summary.branches.total);
	        writer.println('BRH:' + summary.branches.covered);
	        writer.println('end_of_record');
	    }

	    onEnd() {
	        this.contentWriter.close();
	    }
	}

	lcovonly = LcovOnlyReport;
	return lcovonly;
}

var istanbulReports = {
    create(name, cfg) {
        cfg = cfg || {};
        let Cons;
        try {
            Cons = requireLcovonly();
        } catch (e) {
            if (e.code !== 'MODULE_NOT_FOUND') {
                throw e;
            }

            Cons = commonjsRequire(name);
        }

        return new Cons(cfg);
    }
};

/*
* Copyright Node.js contributors. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to
* deal in the Software without restriction, including without limitation the
* rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
* sell copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
* IN THE SOFTWARE.
*/

// TODO(bcoe): this logic is ported from Node.js' internal source map
// helpers:
// https://github.com/nodejs/node/blob/master/lib/internal/source_map/source_map_cache.js
// we should to upstream and downstream fixes.

const { readFileSync: readFileSync$1 } = require$$0$1;
const { fileURLToPath: fileURLToPath$2, pathToFileURL: pathToFileURL$1 } = require$$1$1;
const util$2 = require$$2;
const debuglog$2 = util$2.debuglog('c8');

/**
 * Extract the sourcemap url from a source file
 * reference: https://sourcemaps.info/spec.html
 * @param {String} file - compilation target file
 * @returns {String} full path to source map file
 * @private
 */
function getSourceMapFromFile$1 (filename) {
  const fileBody = readFileSync$1(filename).toString();
  const sourceMapLineRE = /\/[*/]#\s+sourceMappingURL=(?<sourceMappingURL>[^\s]+)/;
  const results = fileBody.match(sourceMapLineRE);
  if (results !== null) {
    const sourceMappingURL = results.groups.sourceMappingURL;
    const sourceMap = dataFromUrl(pathToFileURL$1(filename), sourceMappingURL);
    return sourceMap
  } else {
    return null
  }
}

function dataFromUrl (sourceURL, sourceMappingURL) {
  try {
    const url = new URL(sourceMappingURL);
    switch (url.protocol) {
      case 'data:':
        return sourceMapFromDataUrl(url.pathname)
      default:
        return null
    }
  } catch (err) {
    debuglog$2(err);
    // If no scheme is present, we assume we are dealing with a file path.
    const mapURL = new URL(sourceMappingURL, sourceURL).href;
    return sourceMapFromFile(mapURL)
  }
}

function sourceMapFromFile (mapURL) {
  try {
    const content = readFileSync$1(fileURLToPath$2(mapURL), 'utf8');
    return JSON.parse(content)
  } catch (err) {
    debuglog$2(err);
    return null
  }
}

// data:[<mediatype>][;base64],<data> see:
// https://tools.ietf.org/html/rfc2397#section-2
function sourceMapFromDataUrl (url) {
  const { 0: format, 1: data } = url.split(',');
  const splitFormat = format.split(';');
  const contentType = splitFormat[0];
  const base64 = splitFormat[splitFormat.length - 1] === 'base64';
  if (contentType === 'application/json') {
    const decodedData = base64 ? Buffer.from(data, 'base64').toString('utf8') : data;
    try {
      return JSON.parse(decodedData)
    } catch (err) {
      debuglog$2(err);
      return null
    }
  } else {
    debuglog$2(`unexpected content-type ${contentType}`);
    return null
  }
}

var sourceMapFromFile_1 = getSourceMapFromFile$1;

var convertSourceMap$1 = {};

(function (exports) {
	var fs = require$$0$1;
	var path = require$$0;

	Object.defineProperty(exports, 'commentRegex', {
	  get: function getCommentRegex () {
	    return /^\s*\/(?:\/|\*)[@#]\s+sourceMappingURL=data:(?:application|text)\/json;(?:charset[:=]\S+?;)?base64,(?:.*)$/mg;
	  }
	});

	Object.defineProperty(exports, 'mapFileCommentRegex', {
	  get: function getMapFileCommentRegex () {
	    // Matches sourceMappingURL in either // or /* comment styles.
	    return /(?:\/\/[@#][ \t]+sourceMappingURL=([^\s'"`]+?)[ \t]*$)|(?:\/\*[@#][ \t]+sourceMappingURL=([^\*]+?)[ \t]*(?:\*\/){1}[ \t]*$)/mg;
	  }
	});

	var decodeBase64;
	if (typeof Buffer !== 'undefined') {
	  if (typeof Buffer.from === 'function') {
	    decodeBase64 = decodeBase64WithBufferFrom;
	  } else {
	    decodeBase64 = decodeBase64WithNewBuffer;
	  }
	} else {
	  decodeBase64 = decodeBase64WithAtob;
	}

	function decodeBase64WithBufferFrom(base64) {
	  return Buffer.from(base64, 'base64').toString();
	}

	function decodeBase64WithNewBuffer(base64) {
	  if (typeof value === 'number') {
	    throw new TypeError('The value to decode must not be of type number.');
	  }
	  return new Buffer(base64, 'base64').toString();
	}

	function decodeBase64WithAtob(base64) {
	  return decodeURIComponent(escape(atob(base64)));
	}

	function stripComment(sm) {
	  return sm.split(',').pop();
	}

	function readFromFileMap(sm, dir) {
	  // NOTE: this will only work on the server since it attempts to read the map file

	  var r = exports.mapFileCommentRegex.exec(sm);

	  // for some odd reason //# .. captures in 1 and /* .. */ in 2
	  var filename = r[1] || r[2];
	  var filepath = path.resolve(dir, filename);

	  try {
	    return fs.readFileSync(filepath, 'utf8');
	  } catch (e) {
	    throw new Error('An error occurred while trying to read the map file at ' + filepath + '\n' + e);
	  }
	}

	function Converter (sm, opts) {
	  opts = opts || {};

	  if (opts.isFileComment) sm = readFromFileMap(sm, opts.commentFileDir);
	  if (opts.hasComment) sm = stripComment(sm);
	  if (opts.isEncoded) sm = decodeBase64(sm);
	  if (opts.isJSON || opts.isEncoded) sm = JSON.parse(sm);

	  this.sourcemap = sm;
	}

	Converter.prototype.toJSON = function (space) {
	  return JSON.stringify(this.sourcemap, null, space);
	};

	if (typeof Buffer !== 'undefined') {
	  if (typeof Buffer.from === 'function') {
	    Converter.prototype.toBase64 = encodeBase64WithBufferFrom;
	  } else {
	    Converter.prototype.toBase64 = encodeBase64WithNewBuffer;
	  }
	} else {
	  Converter.prototype.toBase64 = encodeBase64WithBtoa;
	}

	function encodeBase64WithBufferFrom() {
	  var json = this.toJSON();
	  return Buffer.from(json, 'utf8').toString('base64');
	}

	function encodeBase64WithNewBuffer() {
	  var json = this.toJSON();
	  if (typeof json === 'number') {
	    throw new TypeError('The json to encode must not be of type number.');
	  }
	  return new Buffer(json, 'utf8').toString('base64');
	}

	function encodeBase64WithBtoa() {
	  var json = this.toJSON();
	  return btoa(unescape(encodeURIComponent(json)));
	}

	Converter.prototype.toComment = function (options) {
	  var base64 = this.toBase64();
	  var data = 'sourceMappingURL=data:application/json;charset=utf-8;base64,' + base64;
	  return options && options.multiline ? '/*# ' + data + ' */' : '//# ' + data;
	};

	// returns copy instead of original
	Converter.prototype.toObject = function () {
	  return JSON.parse(this.toJSON());
	};

	Converter.prototype.addProperty = function (key, value) {
	  if (this.sourcemap.hasOwnProperty(key)) throw new Error('property "' + key + '" already exists on the sourcemap, use set property instead');
	  return this.setProperty(key, value);
	};

	Converter.prototype.setProperty = function (key, value) {
	  this.sourcemap[key] = value;
	  return this;
	};

	Converter.prototype.getProperty = function (key) {
	  return this.sourcemap[key];
	};

	exports.fromObject = function (obj) {
	  return new Converter(obj);
	};

	exports.fromJSON = function (json) {
	  return new Converter(json, { isJSON: true });
	};

	exports.fromBase64 = function (base64) {
	  return new Converter(base64, { isEncoded: true });
	};

	exports.fromComment = function (comment) {
	  comment = comment
	    .replace(/^\/\*/g, '//')
	    .replace(/\*\/$/g, '');

	  return new Converter(comment, { isEncoded: true, hasComment: true });
	};

	exports.fromMapFileComment = function (comment, dir) {
	  return new Converter(comment, { commentFileDir: dir, isFileComment: true, isJSON: true });
	};

	// Finds last sourcemap comment in file or returns null if none was found
	exports.fromSource = function (content) {
	  var m = content.match(exports.commentRegex);
	  return m ? exports.fromComment(m.pop()) : null;
	};

	// Finds last sourcemap comment in file or returns null if none was found
	exports.fromMapFileSource = function (content, dir) {
	  var m = content.match(exports.mapFileCommentRegex);
	  return m ? exports.fromMapFileComment(m.pop(), dir) : null;
	};

	exports.removeComments = function (src) {
	  return src.replace(exports.commentRegex, '');
	};

	exports.removeMapFileComments = function (src) {
	  return src.replace(exports.mapFileCommentRegex, '');
	};

	exports.generateMapFileComment = function (file, options) {
	  var data = 'sourceMappingURL=' + file;
	  return options && options.multiline ? '/*# ' + data + ' */' : '//# ' + data;
	};
} (convertSourceMap$1));

var branch;
var hasRequiredBranch;

function requireBranch () {
	if (hasRequiredBranch) return branch;
	hasRequiredBranch = 1;
	branch = class CovBranch {
	  constructor (startLine, startCol, endLine, endCol, count) {
	    this.startLine = startLine;
	    this.startCol = startCol;
	    this.endLine = endLine;
	    this.endCol = endCol;
	    this.count = count;
	  }

	  toIstanbul () {
	    const location = {
	      start: {
	        line: this.startLine,
	        column: this.startCol
	      },
	      end: {
	        line: this.endLine,
	        column: this.endCol
	      }
	    };
	    return {
	      type: 'branch',
	      line: this.startLine,
	      loc: location,
	      locations: [Object.assign({}, location)]
	    }
	  }
	};
	return branch;
}

var _function;
var hasRequired_function;

function require_function () {
	if (hasRequired_function) return _function;
	hasRequired_function = 1;
	_function = class CovFunction {
	  constructor (name, startLine, startCol, endLine, endCol, count) {
	    this.name = name;
	    this.startLine = startLine;
	    this.startCol = startCol;
	    this.endLine = endLine;
	    this.endCol = endCol;
	    this.count = count;
	  }

	  toIstanbul () {
	    const loc = {
	      start: {
	        line: this.startLine,
	        column: this.startCol
	      },
	      end: {
	        line: this.endLine,
	        column: this.endCol
	      }
	    };
	    return {
	      name: this.name,
	      decl: loc,
	      loc: loc,
	      line: this.startLine
	    }
	  }
	};
	return _function;
}

var line;
var hasRequiredLine;

function requireLine () {
	if (hasRequiredLine) return line;
	hasRequiredLine = 1;
	line = class CovLine {
	  constructor (line, startCol, lineStr) {
	    this.line = line;
	    // note that startCol and endCol are absolute positions
	    // within a file, not relative to the line.
	    this.startCol = startCol;

	    // the line length itself does not include the newline characters,
	    // these are however taken into account when enumerating absolute offset.
	    const matchedNewLineChar = lineStr.match(/\r?\n$/u);
	    const newLineLength = matchedNewLineChar ? matchedNewLineChar[0].length : 0;
	    this.endCol = startCol + lineStr.length - newLineLength;

	    // we start with all lines having been executed, and work
	    // backwards zeroing out lines based on V8 output.
	    this.count = 1;

	    // set by source.js during parsing, if /* c8 ignore next */ is found.
	    this.ignore = false;
	  }

	  toIstanbul () {
	    return {
	      start: {
	        line: this.line,
	        column: 0
	      },
	      end: {
	        line: this.line,
	        column: this.endCol - this.startCol
	      }
	    }
	  }
	};
	return line;
}

var range = {};

/**
 * ...something resembling a binary search, to find the lowest line within the range.
 * And then you could break as soon as the line is longer than the range...
 */

var hasRequiredRange;

function requireRange () {
	if (hasRequiredRange) return range;
	hasRequiredRange = 1;
	range.sliceRange = (lines, startCol, endCol, inclusive = false) => {
	  let start = 0;
	  let end = lines.length;

	  if (inclusive) {
	    // I consider this a temporary solution until I find an alternaive way to fix the "off by one issue"
	    --startCol;
	  }

	  while (start < end) {
	    let mid = (start + end) >> 1;
	    if (startCol >= lines[mid].endCol) {
	      start = mid + 1;
	    } else if (endCol < lines[mid].startCol) {
	      end = mid - 1;
	    } else {
	      end = mid;
	      while (mid >= 0 && startCol < lines[mid].endCol && endCol >= lines[mid].startCol) {
	        --mid;
	      }
	      start = mid + 1;
	      break
	    }
	  }

	  while (end < lines.length && startCol < lines[end].endCol && endCol >= lines[end].startCol) {
	    ++end;
	  }

	  return lines.slice(start, end)
	};
	return range;
}

var traceMapping_umd = {exports: {}};

var sourcemapCodec_umd = {exports: {}};

var hasRequiredSourcemapCodec_umd;

function requireSourcemapCodec_umd () {
	if (hasRequiredSourcemapCodec_umd) return sourcemapCodec_umd.exports;
	hasRequiredSourcemapCodec_umd = 1;
	(function (module, exports) {
		(function (global, factory) {
		    factory(exports) ;
		})(commonjsGlobal, (function (exports) {
		    const comma = ','.charCodeAt(0);
		    const semicolon = ';'.charCodeAt(0);
		    const chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
		    const intToChar = new Uint8Array(64); // 64 possible chars.
		    const charToInt = new Uint8Array(128); // z is 122 in ASCII
		    for (let i = 0; i < chars.length; i++) {
		        const c = chars.charCodeAt(i);
		        intToChar[i] = c;
		        charToInt[c] = i;
		    }
		    // Provide a fallback for older environments.
		    const td = typeof TextDecoder !== 'undefined'
		        ? /* #__PURE__ */ new TextDecoder()
		        : typeof Buffer !== 'undefined'
		            ? {
		                decode(buf) {
		                    const out = Buffer.from(buf.buffer, buf.byteOffset, buf.byteLength);
		                    return out.toString();
		                },
		            }
		            : {
		                decode(buf) {
		                    let out = '';
		                    for (let i = 0; i < buf.length; i++) {
		                        out += String.fromCharCode(buf[i]);
		                    }
		                    return out;
		                },
		            };
		    function decode(mappings) {
		        const state = new Int32Array(5);
		        const decoded = [];
		        let index = 0;
		        do {
		            const semi = indexOf(mappings, index);
		            const line = [];
		            let sorted = true;
		            let lastCol = 0;
		            state[0] = 0;
		            for (let i = index; i < semi; i++) {
		                let seg;
		                i = decodeInteger(mappings, i, state, 0); // genColumn
		                const col = state[0];
		                if (col < lastCol)
		                    sorted = false;
		                lastCol = col;
		                if (hasMoreVlq(mappings, i, semi)) {
		                    i = decodeInteger(mappings, i, state, 1); // sourcesIndex
		                    i = decodeInteger(mappings, i, state, 2); // sourceLine
		                    i = decodeInteger(mappings, i, state, 3); // sourceColumn
		                    if (hasMoreVlq(mappings, i, semi)) {
		                        i = decodeInteger(mappings, i, state, 4); // namesIndex
		                        seg = [col, state[1], state[2], state[3], state[4]];
		                    }
		                    else {
		                        seg = [col, state[1], state[2], state[3]];
		                    }
		                }
		                else {
		                    seg = [col];
		                }
		                line.push(seg);
		            }
		            if (!sorted)
		                sort(line);
		            decoded.push(line);
		            index = semi + 1;
		        } while (index <= mappings.length);
		        return decoded;
		    }
		    function indexOf(mappings, index) {
		        const idx = mappings.indexOf(';', index);
		        return idx === -1 ? mappings.length : idx;
		    }
		    function decodeInteger(mappings, pos, state, j) {
		        let value = 0;
		        let shift = 0;
		        let integer = 0;
		        do {
		            const c = mappings.charCodeAt(pos++);
		            integer = charToInt[c];
		            value |= (integer & 31) << shift;
		            shift += 5;
		        } while (integer & 32);
		        const shouldNegate = value & 1;
		        value >>>= 1;
		        if (shouldNegate) {
		            value = -0x80000000 | -value;
		        }
		        state[j] += value;
		        return pos;
		    }
		    function hasMoreVlq(mappings, i, length) {
		        if (i >= length)
		            return false;
		        return mappings.charCodeAt(i) !== comma;
		    }
		    function sort(line) {
		        line.sort(sortComparator);
		    }
		    function sortComparator(a, b) {
		        return a[0] - b[0];
		    }
		    function encode(decoded) {
		        const state = new Int32Array(5);
		        const bufLength = 1024 * 16;
		        const subLength = bufLength - 36;
		        const buf = new Uint8Array(bufLength);
		        const sub = buf.subarray(0, subLength);
		        let pos = 0;
		        let out = '';
		        for (let i = 0; i < decoded.length; i++) {
		            const line = decoded[i];
		            if (i > 0) {
		                if (pos === bufLength) {
		                    out += td.decode(buf);
		                    pos = 0;
		                }
		                buf[pos++] = semicolon;
		            }
		            if (line.length === 0)
		                continue;
		            state[0] = 0;
		            for (let j = 0; j < line.length; j++) {
		                const segment = line[j];
		                // We can push up to 5 ints, each int can take at most 7 chars, and we
		                // may push a comma.
		                if (pos > subLength) {
		                    out += td.decode(sub);
		                    buf.copyWithin(0, subLength, pos);
		                    pos -= subLength;
		                }
		                if (j > 0)
		                    buf[pos++] = comma;
		                pos = encodeInteger(buf, pos, state, segment, 0); // genColumn
		                if (segment.length === 1)
		                    continue;
		                pos = encodeInteger(buf, pos, state, segment, 1); // sourcesIndex
		                pos = encodeInteger(buf, pos, state, segment, 2); // sourceLine
		                pos = encodeInteger(buf, pos, state, segment, 3); // sourceColumn
		                if (segment.length === 4)
		                    continue;
		                pos = encodeInteger(buf, pos, state, segment, 4); // namesIndex
		            }
		        }
		        return out + td.decode(buf.subarray(0, pos));
		    }
		    function encodeInteger(buf, pos, state, segment, j) {
		        const next = segment[j];
		        let num = next - state[j];
		        state[j] = next;
		        num = num < 0 ? (-num << 1) | 1 : num << 1;
		        do {
		            let clamped = num & 0b011111;
		            num >>>= 5;
		            if (num > 0)
		                clamped |= 0b100000;
		            buf[pos++] = intToChar[clamped];
		        } while (num > 0);
		        return pos;
		    }

		    exports.decode = decode;
		    exports.encode = encode;

		    Object.defineProperty(exports, '__esModule', { value: true });

		}));
		
} (sourcemapCodec_umd, sourcemapCodec_umd.exports));
	return sourcemapCodec_umd.exports;
}

var resolveUri_umd = {exports: {}};

var hasRequiredResolveUri_umd;

function requireResolveUri_umd () {
	if (hasRequiredResolveUri_umd) return resolveUri_umd.exports;
	hasRequiredResolveUri_umd = 1;
	(function (module, exports) {
		(function (global, factory) {
		    module.exports = factory() ;
		})(commonjsGlobal, (function () {
		    // Matches the scheme of a URL, eg "http://"
		    const schemeRegex = /^[\w+.-]+:\/\//;
		    /**
		     * Matches the parts of a URL:
		     * 1. Scheme, including ":", guaranteed.
		     * 2. User/password, including "@", optional.
		     * 3. Host, guaranteed.
		     * 4. Port, including ":", optional.
		     * 5. Path, including "/", optional.
		     * 6. Query, including "?", optional.
		     * 7. Hash, including "#", optional.
		     */
		    const urlRegex = /^([\w+.-]+:)\/\/([^@/#?]*@)?([^:/#?]*)(:\d+)?(\/[^#?]*)?(\?[^#]*)?(#.*)?/;
		    /**
		     * File URLs are weird. They dont' need the regular `//` in the scheme, they may or may not start
		     * with a leading `/`, they can have a domain (but only if they don't start with a Windows drive).
		     *
		     * 1. Host, optional.
		     * 2. Path, which may include "/", guaranteed.
		     * 3. Query, including "?", optional.
		     * 4. Hash, including "#", optional.
		     */
		    const fileRegex = /^file:(?:\/\/((?![a-z]:)[^/#?]*)?)?(\/?[^#?]*)(\?[^#]*)?(#.*)?/i;
		    var UrlType;
		    (function (UrlType) {
		        UrlType[UrlType["Empty"] = 1] = "Empty";
		        UrlType[UrlType["Hash"] = 2] = "Hash";
		        UrlType[UrlType["Query"] = 3] = "Query";
		        UrlType[UrlType["RelativePath"] = 4] = "RelativePath";
		        UrlType[UrlType["AbsolutePath"] = 5] = "AbsolutePath";
		        UrlType[UrlType["SchemeRelative"] = 6] = "SchemeRelative";
		        UrlType[UrlType["Absolute"] = 7] = "Absolute";
		    })(UrlType || (UrlType = {}));
		    function isAbsoluteUrl(input) {
		        return schemeRegex.test(input);
		    }
		    function isSchemeRelativeUrl(input) {
		        return input.startsWith('//');
		    }
		    function isAbsolutePath(input) {
		        return input.startsWith('/');
		    }
		    function isFileUrl(input) {
		        return input.startsWith('file:');
		    }
		    function isRelative(input) {
		        return /^[.?#]/.test(input);
		    }
		    function parseAbsoluteUrl(input) {
		        const match = urlRegex.exec(input);
		        return makeUrl(match[1], match[2] || '', match[3], match[4] || '', match[5] || '/', match[6] || '', match[7] || '');
		    }
		    function parseFileUrl(input) {
		        const match = fileRegex.exec(input);
		        const path = match[2];
		        return makeUrl('file:', '', match[1] || '', '', isAbsolutePath(path) ? path : '/' + path, match[3] || '', match[4] || '');
		    }
		    function makeUrl(scheme, user, host, port, path, query, hash) {
		        return {
		            scheme,
		            user,
		            host,
		            port,
		            path,
		            query,
		            hash,
		            type: UrlType.Absolute,
		        };
		    }
		    function parseUrl(input) {
		        if (isSchemeRelativeUrl(input)) {
		            const url = parseAbsoluteUrl('http:' + input);
		            url.scheme = '';
		            url.type = UrlType.SchemeRelative;
		            return url;
		        }
		        if (isAbsolutePath(input)) {
		            const url = parseAbsoluteUrl('http://foo.com' + input);
		            url.scheme = '';
		            url.host = '';
		            url.type = UrlType.AbsolutePath;
		            return url;
		        }
		        if (isFileUrl(input))
		            return parseFileUrl(input);
		        if (isAbsoluteUrl(input))
		            return parseAbsoluteUrl(input);
		        const url = parseAbsoluteUrl('http://foo.com/' + input);
		        url.scheme = '';
		        url.host = '';
		        url.type = input
		            ? input.startsWith('?')
		                ? UrlType.Query
		                : input.startsWith('#')
		                    ? UrlType.Hash
		                    : UrlType.RelativePath
		            : UrlType.Empty;
		        return url;
		    }
		    function stripPathFilename(path) {
		        // If a path ends with a parent directory "..", then it's a relative path with excess parent
		        // paths. It's not a file, so we can't strip it.
		        if (path.endsWith('/..'))
		            return path;
		        const index = path.lastIndexOf('/');
		        return path.slice(0, index + 1);
		    }
		    function mergePaths(url, base) {
		        normalizePath(base, base.type);
		        // If the path is just a "/", then it was an empty path to begin with (remember, we're a relative
		        // path).
		        if (url.path === '/') {
		            url.path = base.path;
		        }
		        else {
		            // Resolution happens relative to the base path's directory, not the file.
		            url.path = stripPathFilename(base.path) + url.path;
		        }
		    }
		    /**
		     * The path can have empty directories "//", unneeded parents "foo/..", or current directory
		     * "foo/.". We need to normalize to a standard representation.
		     */
		    function normalizePath(url, type) {
		        const rel = type <= UrlType.RelativePath;
		        const pieces = url.path.split('/');
		        // We need to preserve the first piece always, so that we output a leading slash. The item at
		        // pieces[0] is an empty string.
		        let pointer = 1;
		        // Positive is the number of real directories we've output, used for popping a parent directory.
		        // Eg, "foo/bar/.." will have a positive 2, and we can decrement to be left with just "foo".
		        let positive = 0;
		        // We need to keep a trailing slash if we encounter an empty directory (eg, splitting "foo/" will
		        // generate `["foo", ""]` pieces). And, if we pop a parent directory. But once we encounter a
		        // real directory, we won't need to append, unless the other conditions happen again.
		        let addTrailingSlash = false;
		        for (let i = 1; i < pieces.length; i++) {
		            const piece = pieces[i];
		            // An empty directory, could be a trailing slash, or just a double "//" in the path.
		            if (!piece) {
		                addTrailingSlash = true;
		                continue;
		            }
		            // If we encounter a real directory, then we don't need to append anymore.
		            addTrailingSlash = false;
		            // A current directory, which we can always drop.
		            if (piece === '.')
		                continue;
		            // A parent directory, we need to see if there are any real directories we can pop. Else, we
		            // have an excess of parents, and we'll need to keep the "..".
		            if (piece === '..') {
		                if (positive) {
		                    addTrailingSlash = true;
		                    positive--;
		                    pointer--;
		                }
		                else if (rel) {
		                    // If we're in a relativePath, then we need to keep the excess parents. Else, in an absolute
		                    // URL, protocol relative URL, or an absolute path, we don't need to keep excess.
		                    pieces[pointer++] = piece;
		                }
		                continue;
		            }
		            // We've encountered a real directory. Move it to the next insertion pointer, which accounts for
		            // any popped or dropped directories.
		            pieces[pointer++] = piece;
		            positive++;
		        }
		        let path = '';
		        for (let i = 1; i < pointer; i++) {
		            path += '/' + pieces[i];
		        }
		        if (!path || (addTrailingSlash && !path.endsWith('/..'))) {
		            path += '/';
		        }
		        url.path = path;
		    }
		    /**
		     * Attempts to resolve `input` URL/path relative to `base`.
		     */
		    function resolve(input, base) {
		        if (!input && !base)
		            return '';
		        const url = parseUrl(input);
		        let inputType = url.type;
		        if (base && inputType !== UrlType.Absolute) {
		            const baseUrl = parseUrl(base);
		            const baseType = baseUrl.type;
		            switch (inputType) {
		                case UrlType.Empty:
		                    url.hash = baseUrl.hash;
		                // fall through
		                case UrlType.Hash:
		                    url.query = baseUrl.query;
		                // fall through
		                case UrlType.Query:
		                case UrlType.RelativePath:
		                    mergePaths(url, baseUrl);
		                // fall through
		                case UrlType.AbsolutePath:
		                    // The host, user, and port are joined, you can't copy one without the others.
		                    url.user = baseUrl.user;
		                    url.host = baseUrl.host;
		                    url.port = baseUrl.port;
		                // fall through
		                case UrlType.SchemeRelative:
		                    // The input doesn't have a schema at least, so we need to copy at least that over.
		                    url.scheme = baseUrl.scheme;
		            }
		            if (baseType > inputType)
		                inputType = baseType;
		        }
		        normalizePath(url, inputType);
		        const queryHash = url.query + url.hash;
		        switch (inputType) {
		            // This is impossible, because of the empty checks at the start of the function.
		            // case UrlType.Empty:
		            case UrlType.Hash:
		            case UrlType.Query:
		                return queryHash;
		            case UrlType.RelativePath: {
		                // The first char is always a "/", and we need it to be relative.
		                const path = url.path.slice(1);
		                if (!path)
		                    return queryHash || '.';
		                if (isRelative(base || input) && !isRelative(path)) {
		                    // If base started with a leading ".", or there is no base and input started with a ".",
		                    // then we need to ensure that the relative path starts with a ".". We don't know if
		                    // relative starts with a "..", though, so check before prepending.
		                    return './' + path + queryHash;
		                }
		                return path + queryHash;
		            }
		            case UrlType.AbsolutePath:
		                return url.path + queryHash;
		            default:
		                return url.scheme + '//' + url.user + url.host + url.port + url.path + queryHash;
		        }
		    }

		    return resolve;

		}));
		
} (resolveUri_umd));
	return resolveUri_umd.exports;
}

var hasRequiredTraceMapping_umd;

function requireTraceMapping_umd () {
	if (hasRequiredTraceMapping_umd) return traceMapping_umd.exports;
	hasRequiredTraceMapping_umd = 1;
	(function (module, exports) {
		(function (global, factory) {
		    factory(exports, requireSourcemapCodec_umd(), requireResolveUri_umd()) ;
		})(commonjsGlobal, (function (exports, sourcemapCodec, resolveUri) {
		    function _interopDefaultLegacy (e) { return e && typeof e === 'object' && 'default' in e ? e : { 'default': e }; }

		    var resolveUri__default = /*#__PURE__*/_interopDefaultLegacy(resolveUri);

		    function resolve(input, base) {
		        // The base is always treated as a directory, if it's not empty.
		        // https://github.com/mozilla/source-map/blob/8cb3ee57/lib/util.js#L327
		        // https://github.com/chromium/chromium/blob/da4adbb3/third_party/blink/renderer/devtools/front_end/sdk/SourceMap.js#L400-L401
		        if (base && !base.endsWith('/'))
		            base += '/';
		        return resolveUri__default["default"](input, base);
		    }

		    /**
		     * Removes everything after the last "/", but leaves the slash.
		     */
		    function stripFilename(path) {
		        if (!path)
		            return '';
		        const index = path.lastIndexOf('/');
		        return path.slice(0, index + 1);
		    }

		    const COLUMN = 0;
		    const SOURCES_INDEX = 1;
		    const SOURCE_LINE = 2;
		    const SOURCE_COLUMN = 3;
		    const NAMES_INDEX = 4;
		    const REV_GENERATED_LINE = 1;
		    const REV_GENERATED_COLUMN = 2;

		    function maybeSort(mappings, owned) {
		        const unsortedIndex = nextUnsortedSegmentLine(mappings, 0);
		        if (unsortedIndex === mappings.length)
		            return mappings;
		        // If we own the array (meaning we parsed it from JSON), then we're free to directly mutate it. If
		        // not, we do not want to modify the consumer's input array.
		        if (!owned)
		            mappings = mappings.slice();
		        for (let i = unsortedIndex; i < mappings.length; i = nextUnsortedSegmentLine(mappings, i + 1)) {
		            mappings[i] = sortSegments(mappings[i], owned);
		        }
		        return mappings;
		    }
		    function nextUnsortedSegmentLine(mappings, start) {
		        for (let i = start; i < mappings.length; i++) {
		            if (!isSorted(mappings[i]))
		                return i;
		        }
		        return mappings.length;
		    }
		    function isSorted(line) {
		        for (let j = 1; j < line.length; j++) {
		            if (line[j][COLUMN] < line[j - 1][COLUMN]) {
		                return false;
		            }
		        }
		        return true;
		    }
		    function sortSegments(line, owned) {
		        if (!owned)
		            line = line.slice();
		        return line.sort(sortComparator);
		    }
		    function sortComparator(a, b) {
		        return a[COLUMN] - b[COLUMN];
		    }

		    let found = false;
		    /**
		     * A binary search implementation that returns the index if a match is found.
		     * If no match is found, then the left-index (the index associated with the item that comes just
		     * before the desired index) is returned. To maintain proper sort order, a splice would happen at
		     * the next index:
		     *
		     * ```js
		     * const array = [1, 3];
		     * const needle = 2;
		     * const index = binarySearch(array, needle, (item, needle) => item - needle);
		     *
		     * assert.equal(index, 0);
		     * array.splice(index + 1, 0, needle);
		     * assert.deepEqual(array, [1, 2, 3]);
		     * ```
		     */
		    function binarySearch(haystack, needle, low, high) {
		        while (low <= high) {
		            const mid = low + ((high - low) >> 1);
		            const cmp = haystack[mid][COLUMN] - needle;
		            if (cmp === 0) {
		                found = true;
		                return mid;
		            }
		            if (cmp < 0) {
		                low = mid + 1;
		            }
		            else {
		                high = mid - 1;
		            }
		        }
		        found = false;
		        return low - 1;
		    }
		    function upperBound(haystack, needle, index) {
		        for (let i = index + 1; i < haystack.length; index = i++) {
		            if (haystack[i][COLUMN] !== needle)
		                break;
		        }
		        return index;
		    }
		    function lowerBound(haystack, needle, index) {
		        for (let i = index - 1; i >= 0; index = i--) {
		            if (haystack[i][COLUMN] !== needle)
		                break;
		        }
		        return index;
		    }
		    function memoizedState() {
		        return {
		            lastKey: -1,
		            lastNeedle: -1,
		            lastIndex: -1,
		        };
		    }
		    /**
		     * This overly complicated beast is just to record the last tested line/column and the resulting
		     * index, allowing us to skip a few tests if mappings are monotonically increasing.
		     */
		    function memoizedBinarySearch(haystack, needle, state, key) {
		        const { lastKey, lastNeedle, lastIndex } = state;
		        let low = 0;
		        let high = haystack.length - 1;
		        if (key === lastKey) {
		            if (needle === lastNeedle) {
		                found = lastIndex !== -1 && haystack[lastIndex][COLUMN] === needle;
		                return lastIndex;
		            }
		            if (needle >= lastNeedle) {
		                // lastIndex may be -1 if the previous needle was not found.
		                low = lastIndex === -1 ? 0 : lastIndex;
		            }
		            else {
		                high = lastIndex;
		            }
		        }
		        state.lastKey = key;
		        state.lastNeedle = needle;
		        return (state.lastIndex = binarySearch(haystack, needle, low, high));
		    }

		    // Rebuilds the original source files, with mappings that are ordered by source line/column instead
		    // of generated line/column.
		    function buildBySources(decoded, memos) {
		        const sources = memos.map(buildNullArray);
		        for (let i = 0; i < decoded.length; i++) {
		            const line = decoded[i];
		            for (let j = 0; j < line.length; j++) {
		                const seg = line[j];
		                if (seg.length === 1)
		                    continue;
		                const sourceIndex = seg[SOURCES_INDEX];
		                const sourceLine = seg[SOURCE_LINE];
		                const sourceColumn = seg[SOURCE_COLUMN];
		                const originalSource = sources[sourceIndex];
		                const originalLine = (originalSource[sourceLine] || (originalSource[sourceLine] = []));
		                const memo = memos[sourceIndex];
		                // The binary search either found a match, or it found the left-index just before where the
		                // segment should go. Either way, we want to insert after that. And there may be multiple
		                // generated segments associated with an original location, so there may need to move several
		                // indexes before we find where we need to insert.
		                const index = upperBound(originalLine, sourceColumn, memoizedBinarySearch(originalLine, sourceColumn, memo, sourceLine));
		                insert(originalLine, (memo.lastIndex = index + 1), [sourceColumn, i, seg[COLUMN]]);
		            }
		        }
		        return sources;
		    }
		    function insert(array, index, value) {
		        for (let i = array.length; i > index; i--) {
		            array[i] = array[i - 1];
		        }
		        array[index] = value;
		    }
		    // Null arrays allow us to use ordered index keys without actually allocating contiguous memory like
		    // a real array. We use a null-prototype object to avoid prototype pollution and deoptimizations.
		    // Numeric properties on objects are magically sorted in ascending order by the engine regardless of
		    // the insertion order. So, by setting any numeric keys, even out of order, we'll get ascending
		    // order when iterating with for-in.
		    function buildNullArray() {
		        return { __proto__: null };
		    }

		    const AnyMap = function (map, mapUrl) {
		        const parsed = typeof map === 'string' ? JSON.parse(map) : map;
		        if (!('sections' in parsed))
		            return new TraceMap(parsed, mapUrl);
		        const mappings = [];
		        const sources = [];
		        const sourcesContent = [];
		        const names = [];
		        recurse(parsed, mapUrl, mappings, sources, sourcesContent, names, 0, 0, Infinity, Infinity);
		        const joined = {
		            version: 3,
		            file: parsed.file,
		            names,
		            sources,
		            sourcesContent,
		            mappings,
		        };
		        return exports.presortedDecodedMap(joined);
		    };
		    function recurse(input, mapUrl, mappings, sources, sourcesContent, names, lineOffset, columnOffset, stopLine, stopColumn) {
		        const { sections } = input;
		        for (let i = 0; i < sections.length; i++) {
		            const { map, offset } = sections[i];
		            let sl = stopLine;
		            let sc = stopColumn;
		            if (i + 1 < sections.length) {
		                const nextOffset = sections[i + 1].offset;
		                sl = Math.min(stopLine, lineOffset + nextOffset.line);
		                if (sl === stopLine) {
		                    sc = Math.min(stopColumn, columnOffset + nextOffset.column);
		                }
		                else if (sl < stopLine) {
		                    sc = columnOffset + nextOffset.column;
		                }
		            }
		            addSection(map, mapUrl, mappings, sources, sourcesContent, names, lineOffset + offset.line, columnOffset + offset.column, sl, sc);
		        }
		    }
		    function addSection(input, mapUrl, mappings, sources, sourcesContent, names, lineOffset, columnOffset, stopLine, stopColumn) {
		        if ('sections' in input)
		            return recurse(...arguments);
		        const map = new TraceMap(input, mapUrl);
		        const sourcesOffset = sources.length;
		        const namesOffset = names.length;
		        const decoded = exports.decodedMappings(map);
		        const { resolvedSources, sourcesContent: contents } = map;
		        append(sources, resolvedSources);
		        append(names, map.names);
		        if (contents)
		            append(sourcesContent, contents);
		        else
		            for (let i = 0; i < resolvedSources.length; i++)
		                sourcesContent.push(null);
		        for (let i = 0; i < decoded.length; i++) {
		            const lineI = lineOffset + i;
		            // We can only add so many lines before we step into the range that the next section's map
		            // controls. When we get to the last line, then we'll start checking the segments to see if
		            // they've crossed into the column range. But it may not have any columns that overstep, so we
		            // still need to check that we don't overstep lines, too.
		            if (lineI > stopLine)
		                return;
		            // The out line may already exist in mappings (if we're continuing the line started by a
		            // previous section). Or, we may have jumped ahead several lines to start this section.
		            const out = getLine(mappings, lineI);
		            // On the 0th loop, the section's column offset shifts us forward. On all other lines (since the
		            // map can be multiple lines), it doesn't.
		            const cOffset = i === 0 ? columnOffset : 0;
		            const line = decoded[i];
		            for (let j = 0; j < line.length; j++) {
		                const seg = line[j];
		                const column = cOffset + seg[COLUMN];
		                // If this segment steps into the column range that the next section's map controls, we need
		                // to stop early.
		                if (lineI === stopLine && column >= stopColumn)
		                    return;
		                if (seg.length === 1) {
		                    out.push([column]);
		                    continue;
		                }
		                const sourcesIndex = sourcesOffset + seg[SOURCES_INDEX];
		                const sourceLine = seg[SOURCE_LINE];
		                const sourceColumn = seg[SOURCE_COLUMN];
		                out.push(seg.length === 4
		                    ? [column, sourcesIndex, sourceLine, sourceColumn]
		                    : [column, sourcesIndex, sourceLine, sourceColumn, namesOffset + seg[NAMES_INDEX]]);
		            }
		        }
		    }
		    function append(arr, other) {
		        for (let i = 0; i < other.length; i++)
		            arr.push(other[i]);
		    }
		    function getLine(arr, index) {
		        for (let i = arr.length; i <= index; i++)
		            arr[i] = [];
		        return arr[index];
		    }

		    const LINE_GTR_ZERO = '`line` must be greater than 0 (lines start at line 1)';
		    const COL_GTR_EQ_ZERO = '`column` must be greater than or equal to 0 (columns start at column 0)';
		    const LEAST_UPPER_BOUND = -1;
		    const GREATEST_LOWER_BOUND = 1;
		    /**
		     * Returns the encoded (VLQ string) form of the SourceMap's mappings field.
		     */
		    exports.encodedMappings = void 0;
		    /**
		     * Returns the decoded (array of lines of segments) form of the SourceMap's mappings field.
		     */
		    exports.decodedMappings = void 0;
		    /**
		     * A low-level API to find the segment associated with a generated line/column (think, from a
		     * stack trace). Line and column here are 0-based, unlike `originalPositionFor`.
		     */
		    exports.traceSegment = void 0;
		    /**
		     * A higher-level API to find the source/line/column associated with a generated line/column
		     * (think, from a stack trace). Line is 1-based, but column is 0-based, due to legacy behavior in
		     * `source-map` library.
		     */
		    exports.originalPositionFor = void 0;
		    /**
		     * Finds the generated line/column position of the provided source/line/column source position.
		     */
		    exports.generatedPositionFor = void 0;
		    /**
		     * Finds all generated line/column positions of the provided source/line/column source position.
		     */
		    exports.allGeneratedPositionsFor = void 0;
		    /**
		     * Iterates each mapping in generated position order.
		     */
		    exports.eachMapping = void 0;
		    /**
		     * Retrieves the source content for a particular source, if its found. Returns null if not.
		     */
		    exports.sourceContentFor = void 0;
		    /**
		     * A helper that skips sorting of the input map's mappings array, which can be expensive for larger
		     * maps.
		     */
		    exports.presortedDecodedMap = void 0;
		    /**
		     * Returns a sourcemap object (with decoded mappings) suitable for passing to a library that expects
		     * a sourcemap, or to JSON.stringify.
		     */
		    exports.decodedMap = void 0;
		    /**
		     * Returns a sourcemap object (with encoded mappings) suitable for passing to a library that expects
		     * a sourcemap, or to JSON.stringify.
		     */
		    exports.encodedMap = void 0;
		    class TraceMap {
		        constructor(map, mapUrl) {
		            const isString = typeof map === 'string';
		            if (!isString && map._decodedMemo)
		                return map;
		            const parsed = (isString ? JSON.parse(map) : map);
		            const { version, file, names, sourceRoot, sources, sourcesContent } = parsed;
		            this.version = version;
		            this.file = file;
		            this.names = names;
		            this.sourceRoot = sourceRoot;
		            this.sources = sources;
		            this.sourcesContent = sourcesContent;
		            const from = resolve(sourceRoot || '', stripFilename(mapUrl));
		            this.resolvedSources = sources.map((s) => resolve(s || '', from));
		            const { mappings } = parsed;
		            if (typeof mappings === 'string') {
		                this._encoded = mappings;
		                this._decoded = undefined;
		            }
		            else {
		                this._encoded = undefined;
		                this._decoded = maybeSort(mappings, isString);
		            }
		            this._decodedMemo = memoizedState();
		            this._bySources = undefined;
		            this._bySourceMemos = undefined;
		        }
		    }
		    (() => {
		        exports.encodedMappings = (map) => {
		            var _a;
		            return ((_a = map._encoded) !== null && _a !== void 0 ? _a : (map._encoded = sourcemapCodec.encode(map._decoded)));
		        };
		        exports.decodedMappings = (map) => {
		            return (map._decoded || (map._decoded = sourcemapCodec.decode(map._encoded)));
		        };
		        exports.traceSegment = (map, line, column) => {
		            const decoded = exports.decodedMappings(map);
		            // It's common for parent source maps to have pointers to lines that have no
		            // mapping (like a "//# sourceMappingURL=") at the end of the child file.
		            if (line >= decoded.length)
		                return null;
		            const segments = decoded[line];
		            const index = traceSegmentInternal(segments, map._decodedMemo, line, column, GREATEST_LOWER_BOUND);
		            return index === -1 ? null : segments[index];
		        };
		        exports.originalPositionFor = (map, { line, column, bias }) => {
		            line--;
		            if (line < 0)
		                throw new Error(LINE_GTR_ZERO);
		            if (column < 0)
		                throw new Error(COL_GTR_EQ_ZERO);
		            const decoded = exports.decodedMappings(map);
		            // It's common for parent source maps to have pointers to lines that have no
		            // mapping (like a "//# sourceMappingURL=") at the end of the child file.
		            if (line >= decoded.length)
		                return OMapping(null, null, null, null);
		            const segments = decoded[line];
		            const index = traceSegmentInternal(segments, map._decodedMemo, line, column, bias || GREATEST_LOWER_BOUND);
		            if (index === -1)
		                return OMapping(null, null, null, null);
		            const segment = segments[index];
		            if (segment.length === 1)
		                return OMapping(null, null, null, null);
		            const { names, resolvedSources } = map;
		            return OMapping(resolvedSources[segment[SOURCES_INDEX]], segment[SOURCE_LINE] + 1, segment[SOURCE_COLUMN], segment.length === 5 ? names[segment[NAMES_INDEX]] : null);
		        };
		        exports.allGeneratedPositionsFor = (map, { source, line, column, bias }) => {
		            // SourceMapConsumer uses LEAST_UPPER_BOUND for some reason, so we follow suit.
		            return generatedPosition(map, source, line, column, bias || LEAST_UPPER_BOUND, true);
		        };
		        exports.generatedPositionFor = (map, { source, line, column, bias }) => {
		            return generatedPosition(map, source, line, column, bias || GREATEST_LOWER_BOUND, false);
		        };
		        exports.eachMapping = (map, cb) => {
		            const decoded = exports.decodedMappings(map);
		            const { names, resolvedSources } = map;
		            for (let i = 0; i < decoded.length; i++) {
		                const line = decoded[i];
		                for (let j = 0; j < line.length; j++) {
		                    const seg = line[j];
		                    const generatedLine = i + 1;
		                    const generatedColumn = seg[0];
		                    let source = null;
		                    let originalLine = null;
		                    let originalColumn = null;
		                    let name = null;
		                    if (seg.length !== 1) {
		                        source = resolvedSources[seg[1]];
		                        originalLine = seg[2] + 1;
		                        originalColumn = seg[3];
		                    }
		                    if (seg.length === 5)
		                        name = names[seg[4]];
		                    cb({
		                        generatedLine,
		                        generatedColumn,
		                        source,
		                        originalLine,
		                        originalColumn,
		                        name,
		                    });
		                }
		            }
		        };
		        exports.sourceContentFor = (map, source) => {
		            const { sources, resolvedSources, sourcesContent } = map;
		            if (sourcesContent == null)
		                return null;
		            let index = sources.indexOf(source);
		            if (index === -1)
		                index = resolvedSources.indexOf(source);
		            return index === -1 ? null : sourcesContent[index];
		        };
		        exports.presortedDecodedMap = (map, mapUrl) => {
		            const tracer = new TraceMap(clone(map, []), mapUrl);
		            tracer._decoded = map.mappings;
		            return tracer;
		        };
		        exports.decodedMap = (map) => {
		            return clone(map, exports.decodedMappings(map));
		        };
		        exports.encodedMap = (map) => {
		            return clone(map, exports.encodedMappings(map));
		        };
		        function generatedPosition(map, source, line, column, bias, all) {
		            line--;
		            if (line < 0)
		                throw new Error(LINE_GTR_ZERO);
		            if (column < 0)
		                throw new Error(COL_GTR_EQ_ZERO);
		            const { sources, resolvedSources } = map;
		            let sourceIndex = sources.indexOf(source);
		            if (sourceIndex === -1)
		                sourceIndex = resolvedSources.indexOf(source);
		            if (sourceIndex === -1)
		                return all ? [] : GMapping(null, null);
		            const generated = (map._bySources || (map._bySources = buildBySources(exports.decodedMappings(map), (map._bySourceMemos = sources.map(memoizedState)))));
		            const segments = generated[sourceIndex][line];
		            if (segments == null)
		                return all ? [] : GMapping(null, null);
		            const memo = map._bySourceMemos[sourceIndex];
		            if (all)
		                return sliceGeneratedPositions(segments, memo, line, column, bias);
		            const index = traceSegmentInternal(segments, memo, line, column, bias);
		            if (index === -1)
		                return GMapping(null, null);
		            const segment = segments[index];
		            return GMapping(segment[REV_GENERATED_LINE] + 1, segment[REV_GENERATED_COLUMN]);
		        }
		    })();
		    function clone(map, mappings) {
		        return {
		            version: map.version,
		            file: map.file,
		            names: map.names,
		            sourceRoot: map.sourceRoot,
		            sources: map.sources,
		            sourcesContent: map.sourcesContent,
		            mappings,
		        };
		    }
		    function OMapping(source, line, column, name) {
		        return { source, line, column, name };
		    }
		    function GMapping(line, column) {
		        return { line, column };
		    }
		    function traceSegmentInternal(segments, memo, line, column, bias) {
		        let index = memoizedBinarySearch(segments, column, memo, line);
		        if (found) {
		            index = (bias === LEAST_UPPER_BOUND ? upperBound : lowerBound)(segments, column, index);
		        }
		        else if (bias === LEAST_UPPER_BOUND)
		            index++;
		        if (index === -1 || index === segments.length)
		            return -1;
		        return index;
		    }
		    function sliceGeneratedPositions(segments, memo, line, column, bias) {
		        let min = traceSegmentInternal(segments, memo, line, column, GREATEST_LOWER_BOUND);
		        // We ignored the bias when tracing the segment so that we're guarnateed to find the first (in
		        // insertion order) segment that matched. Even if we did respect the bias when tracing, we would
		        // still need to call `lowerBound()` to find the first segment, which is slower than just looking
		        // for the GREATEST_LOWER_BOUND to begin with. The only difference that matters for us is when the
		        // binary search didn't match, in which case GREATEST_LOWER_BOUND just needs to increment to
		        // match LEAST_UPPER_BOUND.
		        if (!found && bias === LEAST_UPPER_BOUND)
		            min++;
		        if (min === -1 || min === segments.length)
		            return [];
		        // We may have found the segment that started at an earlier column. If this is the case, then we
		        // need to slice all generated segments that match _that_ column, because all such segments span
		        // to our desired column.
		        const matchedColumn = found ? column : segments[min][COLUMN];
		        // The binary search is not guaranteed to find the lower bound when a match wasn't found.
		        if (!found)
		            min = lowerBound(segments, matchedColumn, min);
		        const max = upperBound(segments, matchedColumn, min);
		        const result = [];
		        for (; min <= max; min++) {
		            const segment = segments[min];
		            result.push(GMapping(segment[REV_GENERATED_LINE] + 1, segment[REV_GENERATED_COLUMN]));
		        }
		        return result;
		    }

		    exports.AnyMap = AnyMap;
		    exports.GREATEST_LOWER_BOUND = GREATEST_LOWER_BOUND;
		    exports.LEAST_UPPER_BOUND = LEAST_UPPER_BOUND;
		    exports.TraceMap = TraceMap;

		    Object.defineProperty(exports, '__esModule', { value: true });

		}));
		
} (traceMapping_umd, traceMapping_umd.exports));
	return traceMapping_umd.exports;
}

var source;
var hasRequiredSource;

function requireSource () {
	if (hasRequiredSource) return source;
	hasRequiredSource = 1;
	const CovLine = requireLine();
	const { sliceRange } = requireRange();
	const { originalPositionFor, generatedPositionFor, GREATEST_LOWER_BOUND, LEAST_UPPER_BOUND } = requireTraceMapping_umd();

	source = class CovSource {
	  constructor (sourceRaw, wrapperLength) {
	    sourceRaw = sourceRaw ? sourceRaw.trimEnd() : '';
	    this.lines = [];
	    this.eof = sourceRaw.length;
	    this.shebangLength = getShebangLength(sourceRaw);
	    this.wrapperLength = wrapperLength - this.shebangLength;
	    this._buildLines(sourceRaw);
	  }

	  _buildLines (source) {
	    let position = 0;
	    let ignoreCount = 0;
	    let ignoreAll = false;
	    for (const [i, lineStr] of source.split(/(?<=\r?\n)/u).entries()) {
	      const line = new CovLine(i + 1, position, lineStr);
	      if (ignoreCount > 0) {
	        line.ignore = true;
	        ignoreCount--;
	      } else if (ignoreAll) {
	        line.ignore = true;
	      }
	      this.lines.push(line);
	      position += lineStr.length;

	      const ignoreToken = this._parseIgnore(lineStr);
	      if (!ignoreToken) continue

	      line.ignore = true;
	      if (ignoreToken.count !== undefined) {
	        ignoreCount = ignoreToken.count;
	      }
	      if (ignoreToken.start || ignoreToken.stop) {
	        ignoreAll = ignoreToken.start;
	        ignoreCount = 0;
	      }
	    }
	  }

	  /**
	   * Parses for comments:
	   *    c8 ignore next
	   *    c8 ignore next 3
	   *    c8 ignore start
	   *    c8 ignore stop
	   * @param {string} lineStr
	   * @return {{count?: number, start?: boolean, stop?: boolean}|undefined}
	   */
	  _parseIgnore (lineStr) {
	    const testIgnoreNextLines = lineStr.match(/^\W*\/\* c8 ignore next (?<count>[0-9]+)/);
	    if (testIgnoreNextLines) {
	      return { count: Number(testIgnoreNextLines.groups.count) }
	    }

	    // Check if comment is on its own line.
	    if (lineStr.match(/^\W*\/\* c8 ignore next/)) {
	      return { count: 1 }
	    }

	    if (lineStr.match(/\/\* c8 ignore next/)) {
	      // Won't ignore successive lines, but the current line will be ignored.
	      return { count: 0 }
	    }

	    const testIgnoreStartStop = lineStr.match(/\/\* c8 ignore (?<mode>start|stop)/);
	    if (testIgnoreStartStop) {
	      if (testIgnoreStartStop.groups.mode === 'start') return { start: true }
	      if (testIgnoreStartStop.groups.mode === 'stop') return { stop: true }
	    }
	  }

	  // given a start column and end column in absolute offsets within
	  // a source file (0 - EOF), returns the relative line column positions.
	  offsetToOriginalRelative (sourceMap, startCol, endCol) {
	    const lines = sliceRange(this.lines, startCol, endCol, true);
	    if (!lines.length) return {}

	    const start = originalPositionTryBoth(
	      sourceMap,
	      lines[0].line,
	      Math.max(0, startCol - lines[0].startCol)
	    );
	    if (!(start && start.source)) {
	      return {}
	    }

	    let end = originalEndPositionFor(
	      sourceMap,
	      lines[lines.length - 1].line,
	      endCol - lines[lines.length - 1].startCol
	    );
	    if (!(end && end.source)) {
	      return {}
	    }

	    if (start.source !== end.source) {
	      return {}
	    }

	    if (start.line === end.line && start.column === end.column) {
	      end = originalPositionFor(sourceMap, {
	        line: lines[lines.length - 1].line,
	        column: endCol - lines[lines.length - 1].startCol,
	        bias: LEAST_UPPER_BOUND
	      });
	      end.column -= 1;
	    }

	    return {
	      source: start.source,
	      startLine: start.line,
	      relStartCol: start.column,
	      endLine: end.line,
	      relEndCol: end.column
	    }
	  }

	  relativeToOffset (line, relCol) {
	    line = Math.max(line, 1);
	    if (this.lines[line - 1] === undefined) return this.eof
	    return Math.min(this.lines[line - 1].startCol + relCol, this.lines[line - 1].endCol)
	  }
	};

	// this implementation is pulled over from istanbul-lib-sourcemap:
	// https://github.com/istanbuljs/istanbuljs/blob/master/packages/istanbul-lib-source-maps/lib/get-mapping.js
	//
	/**
	 * AST ranges are inclusive for start positions and exclusive for end positions.
	 * Source maps are also logically ranges over text, though interacting with
	 * them is generally achieved by working with explicit positions.
	 *
	 * When finding the _end_ location of an AST item, the range behavior is
	 * important because what we're asking for is the _end_ of whatever range
	 * corresponds to the end location we seek.
	 *
	 * This boils down to the following steps, conceptually, though the source-map
	 * library doesn't expose primitives to do this nicely:
	 *
	 * 1. Find the range on the generated file that ends at, or exclusively
	 *    contains the end position of the AST node.
	 * 2. Find the range on the original file that corresponds to
	 *    that generated range.
	 * 3. Find the _end_ location of that original range.
	 */
	function originalEndPositionFor (sourceMap, line, column) {
	  // Given the generated location, find the original location of the mapping
	  // that corresponds to a range on the generated file that overlaps the
	  // generated file end location. Note however that this position on its
	  // own is not useful because it is the position of the _start_ of the range
	  // on the original file, and we want the _end_ of the range.
	  const beforeEndMapping = originalPositionTryBoth(
	    sourceMap,
	    line,
	    Math.max(column - 1, 1)
	  );

	  if (beforeEndMapping.source === null) {
	    return null
	  }

	  // Convert that original position back to a generated one, with a bump
	  // to the right, and a rightward bias. Since 'generatedPositionFor' searches
	  // for mappings in the original-order sorted list, this will find the
	  // mapping that corresponds to the one immediately after the
	  // beforeEndMapping mapping.
	  const afterEndMapping = generatedPositionFor(sourceMap, {
	    source: beforeEndMapping.source,
	    line: beforeEndMapping.line,
	    column: beforeEndMapping.column + 1,
	    bias: LEAST_UPPER_BOUND
	  });
	  if (
	  // If this is null, it means that we've hit the end of the file,
	  // so we can use Infinity as the end column.
	    afterEndMapping.line === null ||
	      // If these don't match, it means that the call to
	      // 'generatedPositionFor' didn't find any other original mappings on
	      // the line we gave, so consider the binding to extend to infinity.
	      originalPositionFor(sourceMap, afterEndMapping).line !==
	          beforeEndMapping.line
	  ) {
	    return {
	      source: beforeEndMapping.source,
	      line: beforeEndMapping.line,
	      column: Infinity
	    }
	  }

	  // Convert the end mapping into the real original position.
	  return originalPositionFor(sourceMap, afterEndMapping)
	}

	function originalPositionTryBoth (sourceMap, line, column) {
	  let original = originalPositionFor(sourceMap, {
	    line,
	    column,
	    bias: GREATEST_LOWER_BOUND
	  });
	  if (original.line === null) {
	    original = originalPositionFor(sourceMap, {
	      line,
	      column,
	      bias: LEAST_UPPER_BOUND
	    });
	  }
	  // The source maps generated by https://github.com/istanbuljs/istanbuljs
	  // (using @babel/core 7.7.5) have behavior, such that a mapping
	  // mid-way through a line maps to an earlier line than a mapping
	  // at position 0. Using the line at positon 0 seems to provide better reports:
	  //
	  //     if (true) {
	  //        cov_y5divc6zu().b[1][0]++;
	  //        cov_y5divc6zu().s[3]++;
	  //        console.info('reachable');
	  //     }  else { ... }
	  //  ^  ^
	  // l5  l3
	  const min = originalPositionFor(sourceMap, {
	    line,
	    column: 0,
	    bias: GREATEST_LOWER_BOUND
	  });
	  if (min.line > original.line) {
	    original = min;
	  }
	  return original
	}

	// Not required since Node 12, see: https://github.com/nodejs/node/pull/27375
	const isPreNode12 = /^v1[0-1]\./u.test(process.version);
	function getShebangLength (source) {
	  if (isPreNode12 && source.indexOf('#!') === 0) {
	    const match = source.match(/(?<shebang>#!.*)/);
	    if (match) {
	      return match.groups.shebang.length
	    }
	  } else {
	    return 0
	  }
	}
	return source;
}

var name = "v8-to-istanbul";
var version = "9.1.0";
var description = "convert from v8 coverage format to istanbul's format";
var main = "index.js";
var types = "index.d.ts";
var scripts = {
	fix: "standard --fix",
	snapshot: "TAP_SNAPSHOT=1 tap test/*.js",
	test: "c8 --reporter=html --reporter=text tap --no-coverage test/*.js",
	posttest: "standard",
	coverage: "c8 report --check-coverage"
};
var repository = "istanbuljs/v8-to-istanbul";
var keywords = [
	"istanbul",
	"v8",
	"coverage"
];
var standard = {
	ignore: [
		"**/test/fixtures"
	]
};
var author = "Ben Coe <ben@npmjs.com>";
var license = "ISC";
var dependencies = {
	"@jridgewell/trace-mapping": "^0.3.12",
	"@types/istanbul-lib-coverage": "^2.0.1",
	"convert-source-map": "^1.6.0"
};
var devDependencies = {
	"@types/node": "^18.0.0",
	c8: "^7.2.1",
	semver: "^7.3.2",
	should: "13.2.3",
	"source-map": "^0.7.3",
	standard: "^16.0.4",
	tap: "^16.0.0"
};
var engines = {
	node: ">=10.12.0"
};
var files = [
	"lib/*.js",
	"index.js",
	"index.d.ts"
];
var require$$9 = {
	name: name,
	version: version,
	description: description,
	main: main,
	types: types,
	scripts: scripts,
	repository: repository,
	keywords: keywords,
	standard: standard,
	author: author,
	license: license,
	dependencies: dependencies,
	devDependencies: devDependencies,
	engines: engines,
	files: files
};

const assert = require$$5;
const convertSourceMap = convertSourceMap$1;
const util$1 = require$$2;
const debuglog$1 = util$1.debuglog('c8');
const { dirname, isAbsolute: isAbsolute$1, join, resolve: resolve$1 } = require$$0;
const { fileURLToPath: fileURLToPath$1 } = require$$1$1;
const CovBranch = requireBranch();
const CovFunction = require_function();
const CovSource = requireSource();
const { sliceRange } = requireRange();
const compatError = Error(`requires Node.js ${require$$9.engines.node}`);
let readFile = () => { throw compatError };
try {
  readFile = require('fs').promises.readFile;
} catch (_err) {
  // most likely we're on an older version of Node.js.
}
const { TraceMap } = requireTraceMapping_umd();
const isOlderNode10 = /^v10\.(([0-9]\.)|(1[0-5]\.))/u.test(process.version);
const isNode8 = /^v8\./.test(process.version);

// Injected when Node.js is loading script into isolate pre Node 10.16.x.
// see: https://github.com/nodejs/node/pull/21573.
const cjsWrapperLength = isOlderNode10 ? require$$12.wrapper[0].length : 0;

var v8ToIstanbul$1 = class V8ToIstanbul {
  constructor (scriptPath, wrapperLength, sources, excludePath) {
    assert(typeof scriptPath === 'string', 'scriptPath must be a string');
    assert(!isNode8, 'This module does not support node 8 or lower, please upgrade to node 10');
    this.path = parsePath(scriptPath);
    this.wrapperLength = wrapperLength === undefined ? cjsWrapperLength : wrapperLength;
    this.excludePath = excludePath || (() => false);
    this.sources = sources || {};
    this.generatedLines = [];
    this.branches = {};
    this.functions = {};
    this.covSources = [];
    this.rawSourceMap = undefined;
    this.sourceMap = undefined;
    this.sourceTranspiled = undefined;
    // Indicate that this report was generated with placeholder data from
    // running --all:
    this.all = false;
  }

  async load () {
    const rawSource = this.sources.source || await readFile(this.path, 'utf8');
    this.rawSourceMap = this.sources.sourceMap ||
      // if we find a source-map (either inline, or a .map file) we load
      // both the transpiled and original source, both of which are used during
      // the backflips we perform to remap absolute to relative positions.
      convertSourceMap.fromSource(rawSource) || convertSourceMap.fromMapFileSource(rawSource, dirname(this.path));

    if (this.rawSourceMap) {
      if (this.rawSourceMap.sourcemap.sources.length > 1) {
        this.sourceMap = new TraceMap(this.rawSourceMap.sourcemap);
        if (!this.sourceMap.sourcesContent) {
          this.sourceMap.sourcesContent = await this.sourcesContentFromSources();
        }
        this.covSources = this.sourceMap.sourcesContent.map((rawSource, i) => ({ source: new CovSource(rawSource, this.wrapperLength), path: this.sourceMap.sources[i] }));
        this.sourceTranspiled = new CovSource(rawSource, this.wrapperLength);
      } else {
        const candidatePath = this.rawSourceMap.sourcemap.sources.length >= 1 ? this.rawSourceMap.sourcemap.sources[0] : this.rawSourceMap.sourcemap.file;
        this.path = this._resolveSource(this.rawSourceMap, candidatePath || this.path);
        this.sourceMap = new TraceMap(this.rawSourceMap.sourcemap);

        let originalRawSource;
        if (this.sources.sourceMap && this.sources.sourceMap.sourcemap && this.sources.sourceMap.sourcemap.sourcesContent && this.sources.sourceMap.sourcemap.sourcesContent.length === 1) {
          // If the sourcesContent field has been provided, return it rather than attempting
          // to load the original source from disk.
          // TODO: investigate whether there's ever a case where we hit this logic with 1:many sources.
          originalRawSource = this.sources.sourceMap.sourcemap.sourcesContent[0];
        } else if (this.sources.originalSource) {
          // Original source may be populated on the sources object.
          originalRawSource = this.sources.originalSource;
        } else if (this.sourceMap.sourcesContent && this.sourceMap.sourcesContent[0]) {
          // perhaps we loaded sourcesContent was populated by an inline source map, or .map file?
          // TODO: investigate whether there's ever a case where we hit this logic with 1:many sources.
          originalRawSource = this.sourceMap.sourcesContent[0];
        } else {
          // We fallback to reading the original source from disk.
          originalRawSource = await readFile(this.path, 'utf8');
        }
        this.covSources = [{ source: new CovSource(originalRawSource, this.wrapperLength), path: this.path }];
        this.sourceTranspiled = new CovSource(rawSource, this.wrapperLength);
      }
    } else {
      this.covSources = [{ source: new CovSource(rawSource, this.wrapperLength), path: this.path }];
    }
  }

  async sourcesContentFromSources () {
    const fileList = this.sourceMap.sources.map(relativePath => {
      const realPath = this._resolveSource(this.rawSourceMap, relativePath);
      return readFile(realPath, 'utf-8')
        .then(result => result)
        .catch(err => {
          debuglog$1(`failed to load ${realPath}: ${err.message}`);
        })
    });
    return await Promise.all(fileList)
  }

  destroy () {
    // no longer necessary, but preserved for backwards compatibility.
  }

  _resolveSource (rawSourceMap, sourcePath) {
    if (sourcePath.startsWith('file://')) {
      return fileURLToPath$1(sourcePath)
    }
    sourcePath = sourcePath.replace(/^webpack:\/\//, '');
    const sourceRoot = rawSourceMap.sourcemap.sourceRoot ? rawSourceMap.sourcemap.sourceRoot.replace('file://', '') : '';
    const candidatePath = join(sourceRoot, sourcePath);

    if (isAbsolute$1(candidatePath)) {
      return candidatePath
    } else {
      return resolve$1(dirname(this.path), candidatePath)
    }
  }

  applyCoverage (blocks) {
    blocks.forEach(block => {
      block.ranges.forEach((range, i) => {
        const { startCol, endCol, path, covSource } = this._maybeRemapStartColEndCol(range);
        if (this.excludePath(path)) {
          return
        }
        let lines;
        if (block.functionName === '(empty-report)') {
          // (empty-report), this will result in a report that has all lines zeroed out.
          lines = covSource.lines.filter((line) => {
            line.count = 0;
            return true
          });
          this.all = lines.length > 0;
        } else {
          lines = sliceRange(covSource.lines, startCol, endCol);
        }
        if (!lines.length) {
          return
        }

        const startLineInstance = lines[0];
        const endLineInstance = lines[lines.length - 1];

        if (block.isBlockCoverage) {
          this.branches[path] = this.branches[path] || [];
          // record branches.
          this.branches[path].push(new CovBranch(
            startLineInstance.line,
            startCol - startLineInstance.startCol,
            endLineInstance.line,
            endCol - endLineInstance.startCol,
            range.count
          ));

          // if block-level granularity is enabled, we still create a single
          // CovFunction tracking object for each set of ranges.
          if (block.functionName && i === 0) {
            this.functions[path] = this.functions[path] || [];
            this.functions[path].push(new CovFunction(
              block.functionName,
              startLineInstance.line,
              startCol - startLineInstance.startCol,
              endLineInstance.line,
              endCol - endLineInstance.startCol,
              range.count
            ));
          }
        } else if (block.functionName) {
          this.functions[path] = this.functions[path] || [];
          // record functions.
          this.functions[path].push(new CovFunction(
            block.functionName,
            startLineInstance.line,
            startCol - startLineInstance.startCol,
            endLineInstance.line,
            endCol - endLineInstance.startCol,
            range.count
          ));
        }

        // record the lines (we record these as statements, such that we're
        // compatible with Istanbul 2.0).
        lines.forEach(line => {
          // make sure branch spans entire line; don't record 'goodbye'
          // branch in `const foo = true ? 'hello' : 'goodbye'` as a
          // 0 for line coverage.
          //
          // All lines start out with coverage of 1, and are later set to 0
          // if they are not invoked; line.ignore prevents a line from being
          // set to 0, and is set if the special comment /* c8 ignore next */
          // is used.

          if (startCol <= line.startCol && endCol >= line.endCol && !line.ignore) {
            line.count = range.count;
          }
        });
      });
    });
  }

  _maybeRemapStartColEndCol (range) {
    let covSource = this.covSources[0].source;
    let startCol = Math.max(0, range.startOffset - covSource.wrapperLength);
    let endCol = Math.min(covSource.eof, range.endOffset - covSource.wrapperLength);
    let path = this.path;

    if (this.sourceMap) {
      startCol = Math.max(0, range.startOffset - this.sourceTranspiled.wrapperLength);
      endCol = Math.min(this.sourceTranspiled.eof, range.endOffset - this.sourceTranspiled.wrapperLength);

      const { startLine, relStartCol, endLine, relEndCol, source } = this.sourceTranspiled.offsetToOriginalRelative(
        this.sourceMap,
        startCol,
        endCol
      );

      const matchingSource = this.covSources.find(covSource => covSource.path === source);
      covSource = matchingSource ? matchingSource.source : this.covSources[0].source;
      path = matchingSource ? matchingSource.path : this.covSources[0].path;

      // next we convert these relative positions back to absolute positions
      // in the original source (which is the format expected in the next step).
      startCol = covSource.relativeToOffset(startLine, relStartCol);
      endCol = covSource.relativeToOffset(endLine, relEndCol);
    }

    return {
      path,
      covSource,
      startCol,
      endCol
    }
  }

  getInnerIstanbul (source, path) {
    // We apply the "Resolving Sources" logic (as defined in
    // sourcemaps.info/spec.html) as a final step for 1:many source maps.
    // for 1:1 source maps, the resolve logic is applied while loading.
    //
    // TODO: could we move the resolving logic for 1:1 source maps to the final
    // step as well? currently this breaks some tests in c8.
    let resolvedPath = path;
    if (this.rawSourceMap && this.rawSourceMap.sourcemap.sources.length > 1) {
      resolvedPath = this._resolveSource(this.rawSourceMap, path);
    }

    if (this.excludePath(resolvedPath)) {
      return
    }

    return {
      [resolvedPath]: {
        path: resolvedPath,
        all: this.all,
        ...this._statementsToIstanbul(source, path),
        ...this._branchesToIstanbul(source, path),
        ...this._functionsToIstanbul(source, path)
      }
    }
  }

  toIstanbul () {
    return this.covSources.reduce((istanbulOuter, { source, path }) => Object.assign(istanbulOuter, this.getInnerIstanbul(source, path)), {})
  }

  _statementsToIstanbul (source, path) {
    const statements = {
      statementMap: {},
      s: {}
    };
    source.lines.forEach((line, index) => {
      statements.statementMap[`${index}`] = line.toIstanbul();
      statements.s[`${index}`] = line.count;
    });
    return statements
  }

  _branchesToIstanbul (source, path) {
    const branches = {
      branchMap: {},
      b: {}
    };
    this.branches[path] = this.branches[path] || [];
    this.branches[path].forEach((branch, index) => {
      const srcLine = source.lines[branch.startLine - 1];
      const ignore = srcLine === undefined ? true : srcLine.ignore;
      branches.branchMap[`${index}`] = branch.toIstanbul();
      branches.b[`${index}`] = [ignore ? 1 : branch.count];
    });
    return branches
  }

  _functionsToIstanbul (source, path) {
    const functions = {
      fnMap: {},
      f: {}
    };
    this.functions[path] = this.functions[path] || [];
    this.functions[path].forEach((fn, index) => {
      const srcLine = source.lines[fn.startLine - 1];
      const ignore = srcLine === undefined ? true : srcLine.ignore;
      functions.fnMap[`${index}`] = fn.toIstanbul();
      functions.f[`${index}`] = ignore ? 1 : fn.count;
    });
    return functions
  }
};

function parsePath (scriptPath) {
  return scriptPath.startsWith('file://') ? fileURLToPath$1(scriptPath) : scriptPath
}

const V8ToIstanbul = v8ToIstanbul$1;

var v8ToIstanbul = function (path, wrapperLength, sources, excludePath) {
  return new V8ToIstanbul(path, wrapperLength, sources, excludePath)
};

var isCjsEsmBridge = ({ functions }) => {
  // https://github.com/nodejs/node/blob/v12.1.0/lib/internal/modules/esm/create_dynamic_module.js#L11-L19
  return functions.length === 3 &&
  functions[0].functionName === '' &&
  functions[0].isBlockCoverage === true &&
  functions[1].functionName === 'get' &&
  functions[1].isBlockCoverage === false &&
  functions[2].functionName === 'set' &&
  functions[2].isBlockCoverage === true
};

/**
 * Compares two script coverages.
 *
 * The result corresponds to the comparison of their `url` value (alphabetical sort).
 */
function compareScriptCovs(a, b) {
    if (a.url === b.url) {
        return 0;
    }
    else if (a.url < b.url) {
        return -1;
    }
    else {
        return 1;
    }
}
/**
 * Compares two function coverages.
 *
 * The result corresponds to the comparison of the root ranges.
 */
function compareFunctionCovs(a, b) {
    return compareRangeCovs(a.ranges[0], b.ranges[0]);
}
/**
 * Compares two range coverages.
 *
 * The ranges are first ordered by ascending `startOffset` and then by
 * descending `endOffset`.
 * This corresponds to a pre-order tree traversal.
 */
function compareRangeCovs(a, b) {
    if (a.startOffset !== b.startOffset) {
        return a.startOffset - b.startOffset;
    }
    else {
        return b.endOffset - a.endOffset;
    }
}

function emitForest(trees) {
    return emitForestLines(trees).join("\n");
}
function emitForestLines(trees) {
    const colMap = getColMap(trees);
    const header = emitOffsets(colMap);
    return [header, ...trees.map(tree => emitTree(tree, colMap).join("\n"))];
}
function getColMap(trees) {
    const eventSet = new Set();
    for (const tree of trees) {
        const stack = [tree];
        while (stack.length > 0) {
            const cur = stack.pop();
            eventSet.add(cur.start);
            eventSet.add(cur.end);
            for (const child of cur.children) {
                stack.push(child);
            }
        }
    }
    const events = [...eventSet];
    events.sort((a, b) => a - b);
    let maxDigits = 1;
    for (const event of events) {
        maxDigits = Math.max(maxDigits, event.toString(10).length);
    }
    const colWidth = maxDigits + 3;
    const colMap = new Map();
    for (const [i, event] of events.entries()) {
        colMap.set(event, i * colWidth);
    }
    return colMap;
}
function emitTree(tree, colMap) {
    const layers = [];
    let nextLayer = [tree];
    while (nextLayer.length > 0) {
        const layer = nextLayer;
        layers.push(layer);
        nextLayer = [];
        for (const node of layer) {
            for (const child of node.children) {
                nextLayer.push(child);
            }
        }
    }
    return layers.map(layer => emitTreeLayer(layer, colMap));
}
function parseFunctionRanges(text, offsetMap) {
    const result = [];
    for (const line of text.split("\n")) {
        for (const range of parseTreeLayer(line, offsetMap)) {
            result.push(range);
        }
    }
    result.sort(compareRangeCovs);
    return result;
}
/**
 *
 * @param layer Sorted list of disjoint trees.
 * @param colMap
 */
function emitTreeLayer(layer, colMap) {
    const line = [];
    let curIdx = 0;
    for (const { start, end, count } of layer) {
        const startIdx = colMap.get(start);
        const endIdx = colMap.get(end);
        if (startIdx > curIdx) {
            line.push(" ".repeat(startIdx - curIdx));
        }
        line.push(emitRange(count, endIdx - startIdx));
        curIdx = endIdx;
    }
    return line.join("");
}
function parseTreeLayer(text, offsetMap) {
    const result = [];
    const regex = /\[(\d+)-*\)/gs;
    while (true) {
        const match = regex.exec(text);
        if (match === null) {
            break;
        }
        const startIdx = match.index;
        const endIdx = startIdx + match[0].length;
        const count = parseInt(match[1], 10);
        const startOffset = offsetMap.get(startIdx);
        const endOffset = offsetMap.get(endIdx);
        if (startOffset === undefined || endOffset === undefined) {
            throw new Error(`Invalid offsets for: ${JSON.stringify(text)}`);
        }
        result.push({ startOffset, endOffset, count });
    }
    return result;
}
function emitRange(count, len) {
    const rangeStart = `[${count.toString(10)}`;
    const rangeEnd = ")";
    const hyphensLen = len - (rangeStart.length + rangeEnd.length);
    const hyphens = "-".repeat(Math.max(0, hyphensLen));
    return `${rangeStart}${hyphens}${rangeEnd}`;
}
function emitOffsets(colMap) {
    let line = "";
    for (const [event, col] of colMap) {
        if (line.length < col) {
            line += " ".repeat(col - line.length);
        }
        line += event.toString(10);
    }
    return line;
}
function parseOffsets(text) {
    const result = new Map();
    const regex = /\d+/gs;
    while (true) {
        const match = regex.exec(text);
        if (match === null) {
            break;
        }
        result.set(match.index, parseInt(match[0], 10));
    }
    return result;
}

/**
 * Creates a deep copy of a process coverage.
 *
 * @param processCov Process coverage to clone.
 * @return Cloned process coverage.
 */
function cloneProcessCov(processCov) {
    const result = [];
    for (const scriptCov of processCov.result) {
        result.push(cloneScriptCov(scriptCov));
    }
    return {
        result,
    };
}
/**
 * Creates a deep copy of a script coverage.
 *
 * @param scriptCov Script coverage to clone.
 * @return Cloned script coverage.
 */
function cloneScriptCov(scriptCov) {
    const functions = [];
    for (const functionCov of scriptCov.functions) {
        functions.push(cloneFunctionCov(functionCov));
    }
    return {
        scriptId: scriptCov.scriptId,
        url: scriptCov.url,
        functions,
    };
}
/**
 * Creates a deep copy of a function coverage.
 *
 * @param functionCov Function coverage to clone.
 * @return Cloned function coverage.
 */
function cloneFunctionCov(functionCov) {
    const ranges = [];
    for (const rangeCov of functionCov.ranges) {
        ranges.push(cloneRangeCov(rangeCov));
    }
    return {
        functionName: functionCov.functionName,
        ranges,
        isBlockCoverage: functionCov.isBlockCoverage,
    };
}
/**
 * Creates a deep copy of a function coverage.
 *
 * @param rangeCov Range coverage to clone.
 * @return Cloned range coverage.
 */
function cloneRangeCov(rangeCov) {
    return {
        startOffset: rangeCov.startOffset,
        endOffset: rangeCov.endOffset,
        count: rangeCov.count,
    };
}

class RangeTree {
    constructor(start, end, delta, children) {
        this.start = start;
        this.end = end;
        this.delta = delta;
        this.children = children;
    }
    /**
     * @precodition `ranges` are well-formed and pre-order sorted
     */
    static fromSortedRanges(ranges) {
        let root;
        // Stack of parent trees and parent counts.
        const stack = [];
        for (const range of ranges) {
            const node = new RangeTree(range.startOffset, range.endOffset, range.count, []);
            if (root === undefined) {
                root = node;
                stack.push([node, range.count]);
                continue;
            }
            let parent;
            let parentCount;
            while (true) {
                [parent, parentCount] = stack[stack.length - 1];
                // assert: `top !== undefined` (the ranges are sorted)
                if (range.startOffset < parent.end) {
                    break;
                }
                else {
                    stack.pop();
                }
            }
            node.delta -= parentCount;
            parent.children.push(node);
            stack.push([node, range.count]);
        }
        return root;
    }
    normalize() {
        const children = [];
        let curEnd;
        let head;
        const tail = [];
        for (const child of this.children) {
            if (head === undefined) {
                head = child;
            }
            else if (child.delta === head.delta && child.start === curEnd) {
                tail.push(child);
            }
            else {
                endChain();
                head = child;
            }
            curEnd = child.end;
        }
        if (head !== undefined) {
            endChain();
        }
        if (children.length === 1) {
            const child = children[0];
            if (child.start === this.start && child.end === this.end) {
                this.delta += child.delta;
                this.children = child.children;
                // `.lazyCount` is zero for both (both are after normalization)
                return;
            }
        }
        this.children = children;
        function endChain() {
            if (tail.length !== 0) {
                head.end = tail[tail.length - 1].end;
                for (const tailTree of tail) {
                    for (const subChild of tailTree.children) {
                        subChild.delta += tailTree.delta - head.delta;
                        head.children.push(subChild);
                    }
                }
                tail.length = 0;
            }
            head.normalize();
            children.push(head);
        }
    }
    /**
     * @precondition `tree.start < value && value < tree.end`
     * @return RangeTree Right part
     */
    split(value) {
        let leftChildLen = this.children.length;
        let mid;
        // TODO(perf): Binary search (check overhead)
        for (let i = 0; i < this.children.length; i++) {
            const child = this.children[i];
            if (child.start < value && value < child.end) {
                mid = child.split(value);
                leftChildLen = i + 1;
                break;
            }
            else if (child.start >= value) {
                leftChildLen = i;
                break;
            }
        }
        const rightLen = this.children.length - leftChildLen;
        const rightChildren = this.children.splice(leftChildLen, rightLen);
        if (mid !== undefined) {
            rightChildren.unshift(mid);
        }
        const result = new RangeTree(value, this.end, this.delta, rightChildren);
        this.end = value;
        return result;
    }
    /**
     * Get the range coverages corresponding to the tree.
     *
     * The ranges are pre-order sorted.
     */
    toRanges() {
        const ranges = [];
        // Stack of parent trees and counts.
        const stack = [[this, 0]];
        while (stack.length > 0) {
            const [cur, parentCount] = stack.pop();
            const count = parentCount + cur.delta;
            ranges.push({ startOffset: cur.start, endOffset: cur.end, count });
            for (let i = cur.children.length - 1; i >= 0; i--) {
                stack.push([cur.children[i], count]);
            }
        }
        return ranges;
    }
}

/**
 * Normalizes a process coverage.
 *
 * Sorts the scripts alphabetically by `url`.
 * Reassigns script ids: the script at index `0` receives `"0"`, the script at
 * index `1` receives `"1"` etc.
 * This does not normalize the script coverages.
 *
 * @param processCov Process coverage to normalize.
 */
function normalizeProcessCov(processCov) {
    processCov.result.sort(compareScriptCovs);
    for (const [scriptId, scriptCov] of processCov.result.entries()) {
        scriptCov.scriptId = scriptId.toString(10);
    }
}
/**
 * Normalizes a script coverage.
 *
 * Sorts the function by root range (pre-order sort).
 * This does not normalize the function coverages.
 *
 * @param scriptCov Script coverage to normalize.
 */
function normalizeScriptCov(scriptCov) {
    scriptCov.functions.sort(compareFunctionCovs);
}
/**
 * Normalizes a script coverage deeply.
 *
 * Normalizes the function coverages deeply, then normalizes the script coverage
 * itself.
 *
 * @param scriptCov Script coverage to normalize.
 */
function deepNormalizeScriptCov(scriptCov) {
    for (const funcCov of scriptCov.functions) {
        normalizeFunctionCov(funcCov);
    }
    normalizeScriptCov(scriptCov);
}
/**
 * Normalizes a function coverage.
 *
 * Sorts the ranges (pre-order sort).
 * TODO: Tree-based normalization of the ranges.
 *
 * @param funcCov Function coverage to normalize.
 */
function normalizeFunctionCov(funcCov) {
    funcCov.ranges.sort(compareRangeCovs);
    const tree = RangeTree.fromSortedRanges(funcCov.ranges);
    normalizeRangeTree(tree);
    funcCov.ranges = tree.toRanges();
}
/**
 * @internal
 */
function normalizeRangeTree(tree) {
    tree.normalize();
}

/**
 * Merges a list of process coverages.
 *
 * The result is normalized.
 * The input values may be mutated, it is not safe to use them after passing
 * them to this function.
 * The computation is synchronous.
 *
 * @param processCovs Process coverages to merge.
 * @return Merged process coverage.
 */
function mergeProcessCovs(processCovs) {
    if (processCovs.length === 0) {
        return { result: [] };
    }
    const urlToScripts = new Map();
    for (const processCov of processCovs) {
        for (const scriptCov of processCov.result) {
            let scriptCovs = urlToScripts.get(scriptCov.url);
            if (scriptCovs === undefined) {
                scriptCovs = [];
                urlToScripts.set(scriptCov.url, scriptCovs);
            }
            scriptCovs.push(scriptCov);
        }
    }
    const result = [];
    for (const scripts of urlToScripts.values()) {
        // assert: `scripts.length > 0`
        result.push(mergeScriptCovs(scripts));
    }
    const merged = { result };
    normalizeProcessCov(merged);
    return merged;
}
/**
 * Merges a list of matching script coverages.
 *
 * Scripts are matching if they have the same `url`.
 * The result is normalized.
 * The input values may be mutated, it is not safe to use them after passing
 * them to this function.
 * The computation is synchronous.
 *
 * @param scriptCovs Process coverages to merge.
 * @return Merged script coverage, or `undefined` if the input list was empty.
 */
function mergeScriptCovs(scriptCovs) {
    if (scriptCovs.length === 0) {
        return undefined;
    }
    else if (scriptCovs.length === 1) {
        const merged = scriptCovs[0];
        deepNormalizeScriptCov(merged);
        return merged;
    }
    const first = scriptCovs[0];
    const scriptId = first.scriptId;
    const url = first.url;
    const rangeToFuncs = new Map();
    for (const scriptCov of scriptCovs) {
        for (const funcCov of scriptCov.functions) {
            const rootRange = stringifyFunctionRootRange(funcCov);
            let funcCovs = rangeToFuncs.get(rootRange);
            if (funcCovs === undefined ||
                // if the entry in rangeToFuncs is function-level granularity and
                // the new coverage is block-level, prefer block-level.
                (!funcCovs[0].isBlockCoverage && funcCov.isBlockCoverage)) {
                funcCovs = [];
                rangeToFuncs.set(rootRange, funcCovs);
            }
            else if (funcCovs[0].isBlockCoverage && !funcCov.isBlockCoverage) {
                // if the entry in rangeToFuncs is block-level granularity, we should
                // not append function level granularity.
                continue;
            }
            funcCovs.push(funcCov);
        }
    }
    const functions = [];
    for (const funcCovs of rangeToFuncs.values()) {
        // assert: `funcCovs.length > 0`
        functions.push(mergeFunctionCovs(funcCovs));
    }
    const merged = { scriptId, url, functions };
    normalizeScriptCov(merged);
    return merged;
}
/**
 * Returns a string representation of the root range of the function.
 *
 * This string can be used to match function with same root range.
 * The string is derived from the start and end offsets of the root range of
 * the function.
 * This assumes that `ranges` is non-empty (true for valid function coverages).
 *
 * @param funcCov Function coverage with the range to stringify
 * @internal
 */
function stringifyFunctionRootRange(funcCov) {
    const rootRange = funcCov.ranges[0];
    return `${rootRange.startOffset.toString(10)};${rootRange.endOffset.toString(10)}`;
}
/**
 * Merges a list of matching function coverages.
 *
 * Functions are matching if their root ranges have the same span.
 * The result is normalized.
 * The input values may be mutated, it is not safe to use them after passing
 * them to this function.
 * The computation is synchronous.
 *
 * @param funcCovs Function coverages to merge.
 * @return Merged function coverage, or `undefined` if the input list was empty.
 */
function mergeFunctionCovs(funcCovs) {
    if (funcCovs.length === 0) {
        return undefined;
    }
    else if (funcCovs.length === 1) {
        const merged = funcCovs[0];
        normalizeFunctionCov(merged);
        return merged;
    }
    const functionName = funcCovs[0].functionName;
    const trees = [];
    for (const funcCov of funcCovs) {
        // assert: `fn.ranges.length > 0`
        // assert: `fn.ranges` is sorted
        trees.push(RangeTree.fromSortedRanges(funcCov.ranges));
    }
    // assert: `trees.length > 0`
    const mergedTree = mergeRangeTrees(trees);
    normalizeRangeTree(mergedTree);
    const ranges = mergedTree.toRanges();
    const isBlockCoverage = !(ranges.length === 1 && ranges[0].count === 0);
    const merged = { functionName, ranges, isBlockCoverage };
    // assert: `merged` is normalized
    return merged;
}
/**
 * @precondition Same `start` and `end` for all the trees
 */
function mergeRangeTrees(trees) {
    if (trees.length <= 1) {
        return trees[0];
    }
    const first = trees[0];
    let delta = 0;
    for (const tree of trees) {
        delta += tree.delta;
    }
    const children = mergeRangeTreeChildren(trees);
    return new RangeTree(first.start, first.end, delta, children);
}
class RangeTreeWithParent {
    constructor(parentIndex, tree) {
        this.parentIndex = parentIndex;
        this.tree = tree;
    }
}
class StartEvent {
    constructor(offset, trees) {
        this.offset = offset;
        this.trees = trees;
    }
    static compare(a, b) {
        return a.offset - b.offset;
    }
}
class StartEventQueue {
    constructor(queue) {
        this.queue = queue;
        this.nextIndex = 0;
        this.pendingOffset = 0;
        this.pendingTrees = undefined;
    }
    static fromParentTrees(parentTrees) {
        const startToTrees = new Map();
        for (const [parentIndex, parentTree] of parentTrees.entries()) {
            for (const child of parentTree.children) {
                let trees = startToTrees.get(child.start);
                if (trees === undefined) {
                    trees = [];
                    startToTrees.set(child.start, trees);
                }
                trees.push(new RangeTreeWithParent(parentIndex, child));
            }
        }
        const queue = [];
        for (const [startOffset, trees] of startToTrees) {
            queue.push(new StartEvent(startOffset, trees));
        }
        queue.sort(StartEvent.compare);
        return new StartEventQueue(queue);
    }
    setPendingOffset(offset) {
        this.pendingOffset = offset;
    }
    pushPendingTree(tree) {
        if (this.pendingTrees === undefined) {
            this.pendingTrees = [];
        }
        this.pendingTrees.push(tree);
    }
    next() {
        const pendingTrees = this.pendingTrees;
        const nextEvent = this.queue[this.nextIndex];
        if (pendingTrees === undefined) {
            this.nextIndex++;
            return nextEvent;
        }
        else if (nextEvent === undefined) {
            this.pendingTrees = undefined;
            return new StartEvent(this.pendingOffset, pendingTrees);
        }
        else {
            if (this.pendingOffset < nextEvent.offset) {
                this.pendingTrees = undefined;
                return new StartEvent(this.pendingOffset, pendingTrees);
            }
            else {
                if (this.pendingOffset === nextEvent.offset) {
                    this.pendingTrees = undefined;
                    for (const tree of pendingTrees) {
                        nextEvent.trees.push(tree);
                    }
                }
                this.nextIndex++;
                return nextEvent;
            }
        }
    }
}
function mergeRangeTreeChildren(parentTrees) {
    const result = [];
    const startEventQueue = StartEventQueue.fromParentTrees(parentTrees);
    const parentToNested = new Map();
    let openRange;
    while (true) {
        const event = startEventQueue.next();
        if (event === undefined) {
            break;
        }
        if (openRange !== undefined && openRange.end <= event.offset) {
            result.push(nextChild(openRange, parentToNested));
            openRange = undefined;
        }
        if (openRange === undefined) {
            let openRangeEnd = event.offset + 1;
            for (const { parentIndex, tree } of event.trees) {
                openRangeEnd = Math.max(openRangeEnd, tree.end);
                insertChild(parentToNested, parentIndex, tree);
            }
            startEventQueue.setPendingOffset(openRangeEnd);
            openRange = { start: event.offset, end: openRangeEnd };
        }
        else {
            for (const { parentIndex, tree } of event.trees) {
                if (tree.end > openRange.end) {
                    const right = tree.split(openRange.end);
                    startEventQueue.pushPendingTree(new RangeTreeWithParent(parentIndex, right));
                }
                insertChild(parentToNested, parentIndex, tree);
            }
        }
    }
    if (openRange !== undefined) {
        result.push(nextChild(openRange, parentToNested));
    }
    return result;
}
function insertChild(parentToNested, parentIndex, tree) {
    let nested = parentToNested.get(parentIndex);
    if (nested === undefined) {
        nested = [];
        parentToNested.set(parentIndex, nested);
    }
    nested.push(tree);
}
function nextChild(openRange, parentToNested) {
    const matchingTrees = [];
    for (const nested of parentToNested.values()) {
        if (nested.length === 1 && nested[0].start === openRange.start && nested[0].end === openRange.end) {
            matchingTrees.push(nested[0]);
        }
        else {
            matchingTrees.push(new RangeTree(openRange.start, openRange.end, 0, nested));
        }
    }
    parentToNested.clear();
    return mergeRangeTrees(matchingTrees);
}

var lib = /*#__PURE__*/Object.freeze({
	__proto__: null,
	emitForest: emitForest,
	emitForestLines: emitForestLines,
	parseFunctionRanges: parseFunctionRanges,
	parseOffsets: parseOffsets,
	cloneFunctionCov: cloneFunctionCov,
	cloneProcessCov: cloneProcessCov,
	cloneScriptCov: cloneScriptCov,
	cloneRangeCov: cloneRangeCov,
	compareScriptCovs: compareScriptCovs,
	compareFunctionCovs: compareFunctionCovs,
	compareRangeCovs: compareRangeCovs,
	mergeFunctionCovs: mergeFunctionCovs,
	mergeProcessCovs: mergeProcessCovs,
	mergeScriptCovs: mergeScriptCovs,
	RangeTree: RangeTree
});

var require$$11 = /*@__PURE__*/getAugmentedNamespace(lib);

const Exclude = testExclude;
const libCoverage = istanbulLibCoverage.exports;
const libReport = istanbulLibReport;
const reports = istanbulReports;
const { readdirSync, readFileSync, statSync } = require$$0$1;
const { isAbsolute, resolve, extname } = require$$0;
const { pathToFileURL, fileURLToPath } = require$$1$1;
const getSourceMapFromFile = sourceMapFromFile_1;
// TODO: switch back to @c88/v8-coverage once patch is landed.
const v8toIstanbul = v8ToIstanbul;
const isCjsEsmBridgeCov = isCjsEsmBridge;
const util = require$$2;
const debuglog = util.debuglog('c8');

let Report$1 = class Report {
  constructor ({
    exclude,
    extension,
    excludeAfterRemap,
    include,
    reporter,
    reporterOptions,
    reportsDirectory,
    tempDirectory,
    watermarks,
    omitRelative,
    wrapperLength,
    resolve: resolvePaths,
    all,
    src,
    allowExternal = false,
    skipFull,
    excludeNodeModules
  }) {
    this.reporter = reporter;
    this.reporterOptions = reporterOptions || {};
    this.reportsDirectory = reportsDirectory;
    this.tempDirectory = tempDirectory;
    this.watermarks = watermarks;
    this.resolve = resolvePaths;
    this.exclude = new Exclude({
      exclude: exclude,
      include: include,
      extension: extension,
      relativePath: !allowExternal,
      excludeNodeModules: excludeNodeModules
    });
    this.excludeAfterRemap = excludeAfterRemap;
    this.shouldInstrumentCache = new Map();
    this.omitRelative = omitRelative;
    this.sourceMapCache = {};
    this.wrapperLength = wrapperLength;
    this.all = all;
    this.src = this._getSrc(src);
    this.skipFull = skipFull;
  }

  _getSrc (src) {
    if (typeof src === 'string') {
      return [src]
    } else if (Array.isArray(src)) {
      return src
    } else {
      return [process.cwd()]
    }
  }

  async run () {
    const context = libReport.createContext({
      dir: this.reportsDirectory,
      watermarks: this.watermarks,
      coverageMap: await this.getCoverageMapFromAllCoverageFiles()
    });

    for (const _reporter of this.reporter) {
      reports.create(_reporter, {
        skipEmpty: false,
        skipFull: this.skipFull,
        maxCols: process.stdout.columns || 100,
        ...this.reporterOptions[_reporter]
      }).execute(context);
    }
  }

  async getCoverageMapFromAllCoverageFiles () {
    // the merge process can be very expensive, and it's often the case that
    // check-coverage is called immediately after a report. We memoize the
    // result from getCoverageMapFromAllCoverageFiles() to address this
    // use-case.
    if (this._allCoverageFiles) return this._allCoverageFiles

    const map = libCoverage.createCoverageMap();
    const v8ProcessCov = this._getMergedProcessCov();
    const resultCountPerPath = new Map();
    const possibleCjsEsmBridges = new Map();

    for (const v8ScriptCov of v8ProcessCov.result) {
      try {
        const sources = this._getSourceMap(v8ScriptCov);
        const path = resolve(this.resolve, v8ScriptCov.url);
        const converter = v8toIstanbul(path, this.wrapperLength, sources, (path) => {
          if (this.excludeAfterRemap) {
            return !this._shouldInstrument(path)
          }
        });
        await converter.load();

        if (resultCountPerPath.has(path)) {
          resultCountPerPath.set(path, resultCountPerPath.get(path) + 1);
        } else {
          resultCountPerPath.set(path, 0);
        }

        if (isCjsEsmBridgeCov(v8ScriptCov)) {
          possibleCjsEsmBridges.set(converter, {
            path,
            functions: v8ScriptCov.functions
          });
        } else {
          converter.applyCoverage(v8ScriptCov.functions);
          map.merge(converter.toIstanbul());
        }
      } catch (err) {
        debuglog(`file: ${v8ScriptCov.url} error: ${err.stack}`);
      }
    }

    for (const [converter, { path, functions }] of possibleCjsEsmBridges) {
      if (resultCountPerPath.get(path) <= 1) {
        converter.applyCoverage(functions);
        map.merge(converter.toIstanbul());
      }
    }
    this._allCoverageFiles = map;
    return this._allCoverageFiles
  }

  /**
   * Returns source-map and fake source file, if cached during Node.js'
   * execution. This is used to support tools like ts-node, which transpile
   * using runtime hooks.
   *
   * Note: requires Node.js 13+
   *
   * @return {Object} sourceMap and fake source file (created from line #s).
   * @private
   */
  _getSourceMap (v8ScriptCov) {
    const sources = {};
    const sourceMapAndLineLengths = this.sourceMapCache[pathToFileURL(v8ScriptCov.url).href];
    if (sourceMapAndLineLengths) {
      // See: https://github.com/nodejs/node/pull/34305
      if (!sourceMapAndLineLengths.data) return
      sources.sourceMap = {
        sourcemap: sourceMapAndLineLengths.data
      };
      if (sourceMapAndLineLengths.lineLengths) {
        let source = '';
        sourceMapAndLineLengths.lineLengths.forEach(length => {
          source += `${''.padEnd(length, '.')}\n`;
        });
        sources.source = source;
      }
    }
    return sources
  }

  /**
   * Returns the merged V8 process coverage.
   *
   * The result is computed from the individual process coverages generated
   * by Node. It represents the sum of their counts.
   *
   * @return {ProcessCov} Merged V8 process coverage.
   * @private
   */
  _getMergedProcessCov () {
    const { mergeProcessCovs } = require$$11;
    const v8ProcessCovs = [];
    const fileIndex = new Set(); // Set<string>
    for (const v8ProcessCov of this._loadReports()) {
      if (this._isCoverageObject(v8ProcessCov)) {
        if (v8ProcessCov['source-map-cache']) {
          Object.assign(this.sourceMapCache, this._normalizeSourceMapCache(v8ProcessCov['source-map-cache']));
        }
        v8ProcessCovs.push(this._normalizeProcessCov(v8ProcessCov, fileIndex));
      }
    }

    if (this.all) {
      const emptyReports = [];
      v8ProcessCovs.unshift({
        result: emptyReports
      });
      const workingDirs = this.src;
      const { extension } = this.exclude;
      for (const workingDir of workingDirs) {
        this.exclude.globSync(workingDir).forEach((f) => {
          const fullPath = resolve(workingDir, f);
          if (!fileIndex.has(fullPath)) {
            const ext = extname(fullPath);
            if (extension.includes(ext)) {
              const stat = statSync(fullPath);
              const sourceMap = getSourceMapFromFile(fullPath);
              if (sourceMap) {
                this.sourceMapCache[pathToFileURL(fullPath)] = { data: sourceMap };
              }
              emptyReports.push({
                scriptId: 0,
                url: resolve(fullPath),
                functions: [{
                  functionName: '(empty-report)',
                  ranges: [{
                    startOffset: 0,
                    endOffset: stat.size,
                    count: 0
                  }],
                  isBlockCoverage: true
                }]
              });
            }
          }
        });
      }
    }

    return mergeProcessCovs(v8ProcessCovs)
  }

  /**
   * Make sure v8ProcessCov actually contains coverage information.
   *
   * @return {boolean} does it look like v8ProcessCov?
   * @private
   */
  _isCoverageObject (maybeV8ProcessCov) {
    return maybeV8ProcessCov && Array.isArray(maybeV8ProcessCov.result)
  }

  /**
   * Returns the list of V8 process coverages generated by Node.
   *
   * @return {ProcessCov[]} Process coverages generated by Node.
   * @private
   */
  _loadReports () {
    const reports = [];
    for (const file of readdirSync(this.tempDirectory)) {
      try {
        reports.push(JSON.parse(readFileSync(
          resolve(this.tempDirectory, file),
          'utf8'
        )));
      } catch (err) {
        debuglog(`${err.stack}`);
      }
    }
    return reports
  }

  /**
   * Normalizes a process coverage.
   *
   * This function replaces file URLs (`url` property) by their corresponding
   * system-dependent path and applies the current inclusion rules to filter out
   * the excluded script coverages.
   *
   * The result is a copy of the input, with script coverages filtered based
   * on their `url` and the current inclusion rules.
   * There is no deep cloning.
   *
   * @param v8ProcessCov V8 process coverage to normalize.
   * @param fileIndex a Set<string> of paths discovered in coverage
   * @return {v8ProcessCov} Normalized V8 process coverage.
   * @private
   */
  _normalizeProcessCov (v8ProcessCov, fileIndex) {
    const result = [];
    for (const v8ScriptCov of v8ProcessCov.result) {
      // https://github.com/nodejs/node/pull/35498 updates Node.js'
      // builtin module filenames:
      if (/^node:/.test(v8ScriptCov.url)) {
        v8ScriptCov.url = `${v8ScriptCov.url.replace(/^node:/, '')}.js`;
      }
      if (/^file:\/\//.test(v8ScriptCov.url)) {
        try {
          v8ScriptCov.url = fileURLToPath(v8ScriptCov.url);
          fileIndex.add(v8ScriptCov.url);
        } catch (err) {
          debuglog(`${err.stack}`);
          continue
        }
      }
      if ((!this.omitRelative || isAbsolute(v8ScriptCov.url))) {
        if (this.excludeAfterRemap || this._shouldInstrument(v8ScriptCov.url)) {
          result.push(v8ScriptCov);
        }
      }
    }
    return { result }
  }

  /**
   * Normalizes a V8 source map cache.
   *
   * This function normalizes file URLs to a system-independent format.
   *
   * @param v8SourceMapCache V8 source map cache to normalize.
   * @return {v8SourceMapCache} Normalized V8 source map cache.
   * @private
   */
  _normalizeSourceMapCache (v8SourceMapCache) {
    const cache = {};
    for (const fileURL of Object.keys(v8SourceMapCache)) {
      cache[pathToFileURL(fileURLToPath(fileURL)).href] = v8SourceMapCache[fileURL];
    }
    return cache
  }

  /**
   * this.exclude.shouldInstrument with cache
   *
   * @private
   * @return {boolean}
   */
  _shouldInstrument (filename) {
    const cacheResult = this.shouldInstrumentCache.get(filename);
    if (cacheResult !== undefined) {
      return cacheResult
    }

    const result = this.exclude.shouldInstrument(filename);
    this.shouldInstrumentCache.set(filename, result);
    return result
  }
};

var report = function (opts) {
  return new Report$1(opts)
};

var Report = report;

// bazel will create the COVERAGE_OUTPUT_FILE whilst setting up the sandbox.
// therefore, should be doing a file size check rather than presence.
try {
    const stats = require$$0$1.statSync(process.env.COVERAGE_OUTPUT_FILE);
    if (stats.size != 0) {
        // early exit here does not affect the outcome of the tests.
        // bazel will only execute _lcov_merger when tests pass.
        process.exit(0);
    }
    // in case file doesn't exist or some other error is thrown, just ignore it.
} catch {}

const include = require$$0$1
    .readFileSync(process.env.COVERAGE_MANIFEST)
    .toString('utf8')
    .split('\n')
    .filter((f) => f != '');

// TODO: can or should we instrument files from other repositories as well?
// if so then the path.join call below will yield invalid paths since files will have external/wksp as their prefix.
const pwd = require$$0.join(process.env.RUNFILES, process.env.TEST_WORKSPACE);
process.chdir(pwd);

new Report({
    include: include,
    exclude: include.length === 0 ? ['**'] : [],
    reportsDirectory: process.env.COVERAGE_DIR,
    tempDirectory: process.env.COVERAGE_DIR,
    resolve: '',
    src: pwd,
    all: true,
    reporter: ['lcovonly'],
})
    .run()
    .then(() => {
        require$$0$1.renameSync(
            require$$0.join(process.env.COVERAGE_DIR, 'lcov.info'),
            process.env.COVERAGE_OUTPUT_FILE
        );
    })
    .catch((err) => {
        console.error(err);
        process.exit(1);
    });
