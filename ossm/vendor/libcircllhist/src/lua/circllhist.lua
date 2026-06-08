module(..., package.seeall)
--
-- circllhist.lua
--
-- This module is a wrapper around the ffi_libcircllhist library.
-- It defines an ffi_metatype that provides:
--
-- - constructors that return garbage collected values
--
-- - object like operations on histogram_t values, e.g. `H:count()`
--
-- - sanity checking of arguments.
--
--
-- Copyright (c) 2016-2021, Circonus, Inc.
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--   http://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.

local ffi = require('ffi');
local libhist = require "ffi_libcircllhist"

ffi.cdef[[
void* malloc (size_t size);
void free (void* ptr);
]]

-- From circllhist.c
local HIST_APPROX_QUANTILE_ERRORS = {
  [0]  = "success",
  [-1] = "empty histogram",
  [-2] = "out of order quantile request",
  [-3] = "out of bound quantile"
}

local function isnan(x)
  return not (x == x)
end

--
-- Histogram Class
--
local Circllhist = {}
Circllhist.__index = Circllhist
local ffi_histogram_t = ffi.typeof("histogram_t")

--
-- Initializer
--
-- This function should be called after each import of this module. We don't call it directly during
-- require() phase, since ffi metatables can't be changed after they have been assigned. Delaying
-- the assignment allows users of this module to extend the Circllhist class before setting the
-- metatype.
local is_initialized = false
function Circllhist.init()
  if not is_initialized then
    is_initialized = true
    ffi.metatype(ffi_histogram_t, Circllhist)
  end
end

--
-- Constructor
--

-- Primary Constructor
function Circllhist.new()
  local hist = libhist.hist_alloc()
  assert(hist)
  ffi.gc(hist, libhist.hist_free)
  return hist
end

--- Create circllhist from table of double values
function Circllhist.from_data(data)
  local hist = Circllhist:new()
  for _, y in ipairs(data) do
    libhist.hist_insert(hist, y, 1)
  end
  return hist
end

--- Create circllhist from a table that maps value => count
function Circllhist.from_map(map)
  local hist = Circllhist:new()
  for value, count in pairs(map) do
    libhist.hist_insert(hist, value, count)
  end
  return hist
end

--- Create circllhist from a base64 encoded string
-- string_len is optional
function Circllhist.from_base64(b64_string, string_len)
  local hist = Circllhist:new()
  if string_len == nil then
    string_len = string.len(b64_string)
  end
  libhist.hist_deserialize_b64(hist, b64_string, string_len)
  return hist
end

-- Create circllhist from ffi ptr to a histogram_t structure
-- If the parameter gc == true, we garbage collect the structure
-- after we are done with it.
function Circllhist.from_ffi_ptr(ptr, gc)
  local hist = ffi.cast("histogram_t*", ptr)
  if gc then
    ffi.gc(hist, libhist.hist_free)
  end
  return hist
end

--
-- Static Helper Functions
--
function Circllhist.is_instance(obj)
  return ffi.istype(ffi_histogram_t, obj)
end

function Circllhist.number_to_bucket(d)
  local bucket = libhist.double_to_hist_bucket(d)
  if bucket.exp == 0 and bucket.val == -1 then
    return nil
  end
  return bucket.val/10 * math.pow(10, bucket.exp)
end

function Circllhist.bucket_size(bucket_bound)
  local bucket = libhist.double_to_hist_bucket(bucket_bound)
  return libhist.hist_bucket_to_double_bin_width(bucket)
end

function Circllhist.bucket_mid(bucket_bound)
  local bucket = libhist.double_to_hist_bucket(bucket_bound)
  return libhist.hist_bucket_midpoint(bucket)
end

--
-- Mutators
--

-- clear all data from histogram
function Circllhist:clear()
  libhist.hist_clear(self)
end

-- Merge data from other_hist into self
function Circllhist:merge(other_hist)
  if other_hist == nil then return end
  local other_hist_ptr = ffi.new("const histogram_t*[1]", other_hist)
  libhist.hist_accumulate(self, other_hist_ptr, 1)
end

---- insert data into the histogram
--@param value, this does *not* need to be a bucket boundary
--@param count, how many samples of that value to insert, default = 1
function Circllhist:insert(value, count)
  assert(value)
  libhist.hist_insert(self, value, count or 1)
  return self
end

--
-- Iteration Methods
--
local _hist_iter_next -- forward declaration
_hist_iter_next = function(param, state)
  local idx = state
  if idx > param.limit then
    return nil -- end iteration
  end
  local val, count = param.self:bucket_idx(idx)
  if count == 0 then
    -- this is a tail call. No new stack frame will be allocated.
    return _hist_iter_next(param, idx + 1)
  end
  return idx + 1, val, count
end

-- returns a stateless lua iterator, to be used as:
-- for _, bin, cnt in hist:iter() do ..
function Circllhist:iter()
  local param = { self = self, limit = self:bucket_count()-1 }
  local state = 0
  return _hist_iter_next, param, state
end

--
-- Exporters
--

-- returns histogram as a new map object
function Circllhist:to_map()
  local map = {}
  for _, bin, count in self:iter() do
    if isnan(bin) then
      map['nan'] = count
    else
      map[bin] = count
    end
  end
  return map
end

-- returns histogram as a human readable string
function Circllhist:__tostring()
  local buf = {}
  local total = 0
  for _, bin, count in self:iter() do
    table.insert(buf, string.format("%.2g:%d", bin, count))
    total = total + count
  end
  return string.format("Hist[%d]{%s}", total, table.concat(buf, ", "))
end

-- returns histogram as a base64 encoded string
function Circllhist:base64()
  local buf_length = libhist.hist_serialize_b64_estimate(self)
  local buf = ffi.gc(ffi.C.malloc(buf_length), ffi.C.free)
  assert(buf)
  local string_length = libhist.hist_serialize_b64(self, buf, buf_length)
  return ffi.string(buf, string_length)
end

-- for __tojson
local ffi_bucket_str_buffer = ffi.new("char[128]")
local function ffi_hist_bucket_to_string(ffi_bucket, format)
  if format == "double" then
    local bucket_str_len = libhist.hist_bucket_to_string(ffi_bucket, ffi_bucket_str_buffer)
    return ffi.string(ffi_bucket_str_buffer, bucket_str_len)
  elseif format == "hex" then
    return '0x' .. bit.tohex(bit.bor(ffi_bucket.exp, bit.lshift(ffi_bucket.val,8)), 4)
  else
    return string.format(format, libhist.hist_bucket_to_double(ffi_bucket))
  end
end

--- fast, standartized JSON encoder
function Circllhist:tojson(state)
  local hist_bucket_format = (state and state.hist_bucket_format) or "double"
  local buf = {}
  table.insert(buf,"{")
  local total = self:bucket_count()
  for idx = 0, total-1 do
    local ffi_bucket = ffi.new("hist_bucket_t")
    local ffi_count = ffi.new("uint64_t[1]")
    libhist.hist_bucket_idx_bucket(self, idx, ffi_bucket, ffi_count)
    local bin_count = tonumber(ffi_count[0])
    table.insert(buf,string.format('"%s":%d', ffi_hist_bucket_to_string(ffi_bucket, hist_bucket_format), bin_count))
    if idx < total-1 then
      table.insert(buf, ',')
    end
  end
  table.insert(buf, "}")
  return table.concat(buf)
end
-- This meta function will be used by dkjson
Circllhist.__tojson = Circllhist.tojson


--
-- Methods
--

-- returns the number of used buckets
function Circllhist:bucket_count()
  return libhist.hist_bucket_count(self)
end

-- returns value, count of the bucket with given index
function Circllhist:bucket_idx(bucket_id)
  assert(0 <= bucket_id and bucket_id < self:bucket_count())
  local value =  ffi.new("double[1]")
  local count =  ffi.new("uint64_t[1]")
  libhist.hist_bucket_idx(self, bucket_id, value, count)
  return tonumber(value[0]), tonumber(count[0])
end

-- returns true if the histogram is empty
function Circllhist:isempty()
  -- We don't use hist_bucket_count() here b/c we have seen histograms
  -- with used buckets but 0 total count in the wild before.
  return self:count() == 0
end

function Circllhist:isequal(other_hist)
  if not Circllhist.is_instance(other_hist) then
    return false
  end
  local cnt = self:bucket_count()
  if other_hist:bucket_count() ~= cnt then
    return false
  end
  for i = 0, cnt-1 do
    local b1, c1 = self:bucket_idx(i)
    local b2, c2 = other_hist:bucket_idx(i)
    if not b1 == b2 then return false end
    if not c1 == c2 then return false end
  end
  return true
end

-- returns the number of samples represented by the histogram
function Circllhist:count()
  return tonumber(libhist.hist_sample_count(self))
end

-- returns the approximated standard deviation of the samples represented
-- by the histogram
function Circllhist:stddev()
  return libhist.hist_approx_stddev(self)
end

-- returns the approximated k-th moment of the samples represented
-- by the histogram
function Circllhist:moment(k)
  return libhist.hist_approx_moment(self, k)
end

-- returns the approximated mean of the samples represented
-- by the histogram
function Circllhist:mean()
  return libhist.hist_approx_mean(self)
end

-- returns the approximated sum of of the samples represented
-- by the histogram
function Circllhist:sum()
  return libhist.hist_approx_sum(self)
end

--- returns the approximated quantiles of the samples represented
-- by the histogram
-- - quantiles have to be sorted in ascending order
-- - We compute Type-1 quantiles of the Hyndman Fan list
-- - returns nil if the histogram is empty
function Circllhist:quantiles(...)
  local quantiles = {...}
  local q_in = ffi.new("double[?]", #quantiles, quantiles)
  local q_out = ffi.new("double[?]", #quantiles, 0/0)
  local rc = libhist.hist_approx_quantile(self, q_in, #quantiles, q_out)
  if rc ~= 0 then
    rc = HIST_APPROX_QUANTILE_ERRORS[rc] or tostring(rc)
    error("hist_approx_quantile failed with: " .. rc)
  end
  local out = {}
  for i=1,#quantiles do
    out[i] = q_out[i-1]
  end
  return unpack(out)
end

-- returns ratios of samples that are lower than the provided values.
-- E.g.
-- a, b = inverse_quantiles(10, 15)
-- a = ratio of samples <10
-- b = ratio of samples <15
function Circllhist:inverse_quantiles(...)
  local thresholds = {...}
  local iq_in = ffi.new("double[?]", #thresholds, thresholds)
  local iq_out = ffi.new("double[?]", #thresholds, 0/0)
  local rc = libhist.hist_approx_inverse_quantile(self, iq_in, #thresholds, iq_out)
  if rc ~= 0 then
    rc = HIST_APPROX_QUANTILE_ERRORS[rc] or tostring(rc)
    error("hist_approx_inverse_quantile failed with: " .. rc)
  end
  local out = {}
  for i = 1, #thresholds do
    out[i] = iq_out[i-1]
  end
  return unpack(out)
end

--- returns the number of values in a histogram that are in buckets
--- that are entirely lower or equal than a given value.
function Circllhist:count_below(...)
  local out = {}
  for i = 1, select("#", ...) do
    out[i] = libhist.hist_approx_count_below(self, select(i, ...))
  end
  return unpack(out)
end

--- returns the number of values in a histogram that are in buckets
--- that are entirely larger or equal than a given value.
function Circllhist:count_above(...)
  local out = {}
  for i = 1, select("#", ...) do
    out[i] = libhist.hist_approx_count_above(self, select(i, ...))
  end
  return unpack(out)
end

--- returns the number of values in a histogram that are in buckets
--- the same bucket as the provided values
function Circllhist:count_nearby(...)
  local out = {}
  for i = 1, select("#", ...) do
    out[i] = libhist.hist_approx_count_nearby(self, select(i, ...))
  end
  return unpack(out)
end

--- returns a compressed version of the histogram, where
-- bins with exponents smaller than "mbe" have been squashed
-- into 0.
function Circllhist:compress_mbe(mbe)
  local hist_ptr = libhist.hist_compress_mbe(self, mbe)
  return Circllhist.from_ffi_ptr(hist_ptr, true)
end

return Circllhist
