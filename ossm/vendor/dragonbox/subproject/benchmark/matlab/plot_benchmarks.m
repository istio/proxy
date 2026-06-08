% Copyright 2020-2021 Junekey Jeon
%
% The contents of this file may be used under the terms of
% the Apache License v2.0 with LLVM Exceptions.
%
%    (See accompanying file LICENSE-Apache or copy at
%     https://llvm.org/foundation/relicensing/LICENSE.txt)
%
% Alternatively, the contents of this file may be used under the terms of
% the Boost Software License, Version 1.0.
%    (See accompanying file LICENSE-Boost or copy at
%     https://www.boost.org/LICENSE_1_0.txt)
%
% Unless required by applicable law or agreed to in writing, this software
% is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
% KIND, either express or implied.

addpath('../../3rdparty/shaded_plots');
plot_uniform_benchmark('../results/uniform_benchmark_binary32.csv', 32);
plot_uniform_benchmark('../results/uniform_benchmark_binary64.csv', 64);
avg32 = plot_digit_benchmark('../results/digits_benchmark_binary32.csv');
avg64 = plot_digit_benchmark('../results/digits_benchmark_binary64.csv');