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

function [avg] = plot_digit_benchmark( filename )

output_filename = replace(filename, '.csv', '.pdf');

samples_per_digits = csvread(filename,0,1,[0 1 0 1]);
table = readtable(filename);
max_digits = table{size(table,1),2};
number_of_algorithms = size(table,1) / samples_per_digits / max_digits;
names = string(zeros(number_of_algorithms, 1));
for i=1:number_of_algorithms
    names(i) = string(table{samples_per_digits * i * max_digits,1});
end

% algorithm index, digits, measured_time
data = zeros(number_of_algorithms, max_digits, samples_per_digits);
for algorithm_idx=1:number_of_algorithms
    for digits_idx=1:max_digits
        offset = ((algorithm_idx-1) * max_digits + (digits_idx-1)) * samples_per_digits;
        data(algorithm_idx, digits_idx,:) = table{offset+1:offset+samples_per_digits, 4};
    end
end

color_array = {[.8 .06 .1],[.1 .7 .06],[.06 .1 .8],[.6 .2 .8],[.8 .9 0],[.5 .6 .7],[.8 .2 .6]};
avg = zeros(number_of_algorithms, max_digits);
for algorithm_idx=1:number_of_algorithms
    for digits_idx=1:max_digits
        avg(algorithm_idx,digits_idx) = mean(data(algorithm_idx,digits_idx,:));
    end
end

% we will call this function stdshade
addpath(genpath('shaded_plots'));

% plot
fig = figure('Color','w');
fig_handles = zeros(number_of_algorithms,1);
hold on
for algorithm_idx=1:number_of_algorithms
    fig_handles(algorithm_idx) = plot(1:max_digits,squeeze(avg(algorithm_idx,:)), ...
        'Color', color_array{algorithm_idx}, 'LineWidth', 1.2);
end
for algorithm_idx=1:number_of_algorithms
    plot_distribution_prctile(1:max_digits,squeeze(data(algorithm_idx,:,:))', ...
        'Color', color_array{algorithm_idx}, 'Alpha', 0.05, ...
        'Prctile', [30 50 70], 'LineWidth', 0.2);
end
legend(fig_handles,names);
axis([1 max_digits -Inf Inf]);
xlabel('Number of digits');
ylabel('Time (ns)');
h = gca;
h.XTick = 1:max_digits;
h.XGrid = 'on';
h.YGrid = 'on';
set(gca,'TickLabelInterpreter', 'latex');
set(gcf, 'Position', [100 100 1200 500]);
orient(fig,'landscape');
exportgraphics(fig, output_filename, 'BackgroundColor', 'none');
hold off

end
