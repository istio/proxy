module(..., package.seeall)

local Circllhist = require("circllhist")
Circllhist.init()

local writer = io.write

local function assert(cond)
  if not cond then
    error("Assertion Failed")
  end
  writer(".")
end

local function isnan(x)
  return not (x == x)
end

local function sim(x, y)
  return string.format("%.3f", x) == string.format("%.3f", y)
end

local function assert_sim(a,b)
  if not sim(a,b) then
    error(string.format("Expected %f got %f\n", a, b))
  end
end

local function setup(scratch)
  scratch.hist = Circllhist.new();
end

local function teardown(scratch)
end

local function randhist(N)
  local hist = Circllhist:new()
  for i=1,N do
    x = 10^math.random(-100,100) * math.random(1,99)
    hist:insert(x)
  end
  return hist
end

local tests = {}

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

function tests.empty(scratch)
  local hist = scratch.hist
  assert(Circllhist.is_instance(hist))
  for _,b,v in hist:iter() do
    error("We should not have any buckets")
  end
  assert(hist:bucket_count() == 0)
  assert(hist:count() == 0)
  assert(isnan(hist:mean()))
  assert(isnan(hist:stddev()))
  assert(hist:sum() == 0)
  assert(isnan(hist:moment(0)))
  assert(isnan(hist:moment(1)))
  assert(isnan(hist:moment(2)))
  assert(isnan(hist:quantiles(0)))
  assert(isnan(hist:inverse_quantiles(100)))
  assert(hist:count_below(100) == 0)
  assert(hist:count_above(100) == 0)
  assert(hist:count_nearby(100) == 0)
end

function tests.single(scratch)
  local hist = scratch.hist
  hist:insert(100)
  assert(Circllhist.is_instance(hist))
  local flag = false
  for _,b, v in hist:iter() do
    assert(b == 100)
    assert(v == 1)
    flag = true
  end
  assert(flag)
  local mid = Circllhist.bucket_mid(100)
  local bin, cnt = hist:bucket_idx(0)
  assert(bin == 100)
  assert(cnt == 1)
  assert(hist:bucket_count() == 1)
  assert(hist:count() == 1)
  assert_sim(hist:mean(), mid)
  assert(hist:stddev() == 0)
  assert_sim(hist:sum(), mid)
  assert(hist:moment(0) == 1)
  assert_sim(hist:moment(1), mid )
  assert_sim(hist:moment(2), mid * mid)
  assert_sim(hist:quantiles(0), 105)
  assert(hist:inverse_quantiles(100) == 0)
  assert(hist:count_below(100) == 1)
  assert(hist:count_above(100) == 1)
  assert(hist:count_nearby(100) == 1)
end

function tests.quantile_single(scratch)
  local hist = scratch.hist
  hist:insert(100)

  -- a single sample will be located at the bin midpoint
  local mid = 105
  local q = {0, .1, .2, .5, .8, .9, 1}
  local Y = {hist:quantiles(unpack(q))}
  for _,y in ipairs(Y) do
    assert_sim(y, mid)
  end
end

function tests.quantile_two(scratch)
  local hist = scratch.hist
  hist:insert(100, 2)
  -- We expect quantiles at 100 + k*10/3, k=1,2
  local ml = 100 + 10/3
  local mh  = 100 + 2*10/3
  local q = {0,  .1, .2, .5, .50005, .6, .9,  1}
  local Z = {ml, ml, ml, ml,     mh, mh, mh, mh}
  local Y = {hist:quantiles(unpack(q))}
  for i,z in ipairs(Z) do
    assert(sim(Y[i],z))
  end
end

function tests.quantile_multi_bin(scratch)
  local hist = scratch.hist
  hist:insert(100, 1)
  hist:insert(110, 1)
  local q = {0,    .1,  .2,  .5,  .6,  .9,   1 }
  local Z = {105, 105, 105, 105, 115, 115, 115 }
  local Y = {hist:quantiles(unpack(q))}
  for i,z in ipairs(Z) do
    assert(sim(Y[i],z))
  end
end

function tests.quantile_multi_bin_3(scratch)
  local hist = scratch.hist
  hist:insert(100, 1)
  hist:insert(110, 1)
  hist:insert(200, 1)
  local q = {0,    .1,  .3,  .5,  .8,  .9,   1 }
  local Z = {105, 105, 105, 115, 205, 205, 205 }
  local Y = {hist:quantiles(unpack(q))}
  for i,z in ipairs(Z) do
    assert(sim(Y[i],z))
  end
end

function tests.equal(scratch)
  local h = Circllhist:new()
  local k = Circllhist:new()
  assert(h:isequal(k))
  h:insert(100)
  k:insert(100)
  assert(h:isequal(k))
  h:insert(0)
  assert(not h:isequal(k))
end

function tests.base64(scratch)
  local hist = randhist(100)
  local other_hist = Circllhist.from_base64(hist:base64())
  assert(hist:isequal(other_hist))
end

function tests.map(scratch)
  local hist = randhist(100)
  local map = hist:to_map()
  local other_hist = Circllhist.from_map(map)
  assert(hist:isequal(other_hist))
end

function tests.from_data(scratch)
  local hist = scratch.hist
  hist:insert(1)
  hist:insert(2)
  hist:insert(3)
  local other_hist = Circllhist.from_data({1,2,3})
  assert(hist:isequal(other_hist))
end

function tests.merge(scratch)
  local hist = scratch.hist
  hist:insert(1)
  hist:insert(2)
  hist:insert(3)
  local hist2 = Circllhist.from_data({1,2,3})
  hist:merge(hist2)
  hist:merge(hist2)
  local other_hist = Circllhist.from_map({
      [1] = 3,
      [2] = 3,
      [3] = 3,
  })
  assert(hist:isequal(other_hist))
end

function tests.tojson(scratch)
  local hist = Circllhist.from_data { 0, 1, 2, 3, 100, -100 }
  local json = hist:tojson()
  assert([[{"-10e+001":1,"0":1,"+10e-001":1,"+20e-001":1,"+30e-001":1,"+10e+001":1}]] == json)
end

function tests.mbe(scratch)
  local hist = Circllhist.from_data { 10^1, 10^2, 10^3, 10^4 }
  local other_hist = Circllhist.from_data { 0, 0, 10^3, 10^4 }
  assert(other_hist:isequal(hist:compress_mbe(3)))
end

function tests.mid(scratch)
  local function mid(a,b) return 2 * a * b / (a+b) end
  assert_sim(Circllhist.bucket_mid(0), 0)
  assert_sim(Circllhist.bucket_mid(100), mid(100, 110))
  assert_sim(Circllhist.bucket_mid(900), mid(900, 910))
  assert_sim(Circllhist.bucket_mid(990), mid(990, 1000))
  assert_sim(Circllhist.bucket_mid(-100), mid(-100, -110))
end
