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

function [] = plot_uniform_benchmark( filename, bits )

output_filename = replace(filename, '.csv', '.pdf');

samples = csvread(filename,0,1,[0 1 0 1]);
table = readtable(filename);
number_of_algorithms = size(table,1) / samples;
names = string(zeros(number_of_algorithms, 1));
for i=1:number_of_algorithms
    names(i) = string(table{samples * i,1});
end

% algorithm index, measured_time
measured_times = zeros(number_of_algorithms, samples);
for algorithm_idx=1:number_of_algorithms
    offset = (algorithm_idx-1) * samples;
    measured_times(algorithm_idx,:) = table{offset+1:offset+samples, 3};
end
bit_representations = sscanf(strjoin(string(table{1:samples, 2})', ' '), '%lu');

color_array = {[.8 .06 .1],[.1 .7 .06],[.06 .1 .8],[.6 .2 .8],[.8 .9 0],[.5 .6 .7],[.8 .2 .6]};

% compute statistics
av = mean(measured_times');
st = std(measured_times');
me = median(measured_times');

decorated_names = names;
for algorithm_idx=1:number_of_algorithms
    decorated_names(algorithm_idx) = sprintf('%s (avg: %.2f, std: %.2f, med: %.2f)', ...
        names(algorithm_idx), av(algorithm_idx), st(algorithm_idx), me(algorithm_idx));
end

% sample data for plot
maximum_plot_size = 10000;
if samples > maximum_plot_size
    plot_size = maximum_plot_size;
    unit = floor(samples / maximum_plot_size);
    sampled_br = bit_representations(1:unit:unit*plot_size);
    sampled_mt = measured_times(:,1:unit:unit*plot_size);
else
    plot_size = samples;
    sampled_br = bit_representations;
    sampled_mt = measured_times;
end

% plot
fig = figure('Color','w');
fig_handles = zeros(number_of_algorithms,1);
hold on
sz = ones(plot_size, 1) * 0.4;
for algorithm_idx=1:number_of_algorithms
    fig_handles(algorithm_idx) = scatter(sampled_br, ...
        sampled_mt(algorithm_idx,:), sz, '+', ...
        'MarkerEdgeColor', color_array{algorithm_idx}, 'LineWidth', 0.1);
end
legend(fig_handles,decorated_names);
xlabel('Bit representation');
ylabel('Time (ns)');
range = prctile(measured_times(:), 99.9);
axis([0 uint64(2)^bits 0 range(1)]);
if bits==32
    xticks([0 uint64(2)^30 uint64(2)^31 3*uint64(2)^30 uint64(2)^32]);
    xticklabels({'$0$','$2^{30}$','$2^{31}$','$3\times2^{30}$','$2^{32}$'});
elseif bits==64
    xticks([0 uint64(2)^62 uint64(2)^63 3*uint64(2)^62 uint64(2)^64]);
    xticklabels({'$0$','$2^{62}$','$2^{63}$','$3\times2^{62}$','$2^{64}$'});
end
h = gca;
h.XGrid = 'off';
h.YGrid = 'on';
set(gca,'TickLabelInterpreter', 'latex');
set(gcf, 'Position', [100 100 1200 500]);
orient(fig,'landscape');
exportgraphics(fig, output_filename, 'BackgroundColor', 'none');
hold off

end
