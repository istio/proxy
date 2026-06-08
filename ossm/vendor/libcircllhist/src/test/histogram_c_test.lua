module(..., package.seeall)


local ffi = require('ffi');
local libhist = require "ffi_libcircllhist"

local writer = io.write

local function assert(cond)
  if not cond then
    error("Assertion Failed")
  end
  writer(".")
end

local function setup(scratch)
  scratch.hist = libhist.hist_alloc();
end

local function teardown(scratch)
  libhist.hist_free(scratch.hist);
end

local tests = {}
function tests.default_empty(scratch)
  assert(libhist.hist_bucket_count(scratch.hist) == 0)
end

function tests.single_bucket(scratch)
  local hist = scratch.hist
  libhist.hist_insert(hist, 100, 5)

  local value =  ffi.new("double[1]")
  local count =  ffi.new("uint64_t[1]")
  libhist.hist_bucket_idx(hist, 0, value, count)
  assert(value[0] == 100)
  assert(count[0] == 5)

  mid = 100 + 10 * (100 / (100+110))
  assert(math.abs(libhist.hist_approx_sum(hist) - 5*mid) < 0.001)
  assert(math.abs(libhist.hist_approx_mean(hist) - mid) < 0.001)
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
