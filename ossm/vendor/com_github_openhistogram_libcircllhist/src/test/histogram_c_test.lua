module(..., package.seeall)


local ffi = require('ffi');
local circllhist = require "ffi_libcircllhist"

local writer = io.write

local function assert(cond)
  if not cond then
    error("Assertion Failed")
  end
  writer(".")
end

local function setup(scratch)
  scratch.hist = circllhist.hist_alloc();
end

local function teardown(scratch)
  circllhist.hist_free(scratch.hist);
end

local tests = {}
function tests.default_empty(scratch)
  assert(circllhist.hist_bucket_count(scratch.hist) == 0)
end

function tests.single_bucket(scratch)
  local hist = scratch.hist
  circllhist.hist_insert(hist, 3.1, 5)

  assert(circllhist.hist_bucket_count(hist) == 1)
  assert(circllhist.hist_approx_mean(hist) == 3.15)

  local value =  ffi.new("double[1]")
  local count =  ffi.new("uint64_t[1]")
  circllhist.hist_bucket_idx(hist, 0, value, count)
  assert(value[0] == 3.1)
  assert(count[0] == 5)
end

function runTests()
  for test_name,test in pairs(tests) do
    writer("Test " .. test_name .. ": ")
    local scratch = {}
    setup(scratch)
    test(scratch)
    teardown(scratch)
    writer(" SUCCESS\n")
  end
end
