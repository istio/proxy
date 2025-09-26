%% Shaded line plot
% Example showing the difference between the standard plot routine and the
% shaded routine

x = -2*pi:pi/100:2*pi;
fx = sin(x);

figure('Color','w');
subplot(1,2,1);
hold on
plot(x,fx);
plot(2*x+pi/2,0.5*fx+0.1*x);
hold off
title('plot');

subplot(1,2,2);
hold on
plot_shaded(x,fx);
plot_shaded(2*x+pi/2,0.5*fx+0.1*x);
hold off
title('plot\_shaded');

%% Histogram plot
% Plots two histograms for two different distributions

X1 = 3 + 2.0*randn([100000,1]);
X2 = 12 + 4.0*randn([100000,1]);

figure('Color','w');
hold on
plot_histogram_shaded(X1,'Alpha',0.3);
plot_histogram_shaded(X2);
hold off
title('plot\_histogram\_shaded');


%% Distribution plots
% Show different plot routines to visualize measurement errors/noise

X = 1:0.25:10;
Y = sin(X)+0.25*X;
Y_error = randn(1000,numel(Y));
Y_noisy = Y+Y_error.*repmat(0.1*X,[size(Y_error,1) 1]);


figure('Color','w');
subplot(3,1,1);
plot(X,Y,'LineWidth',1.5);
title('plot (True value y=f(x))');
ylim([-1 5]);

subplot(3,1,2);
hold on
plot(X,Y,'LineWidth',1.5);
plot_distribution(X,Y_noisy);
hold off
title('plot\_distribution');
ylim([-1 5]);

subplot(3,1,3);
hold on
plot(X,Y,'LineWidth',1.5);
plot_distribution_prctile(X,Y_noisy,'Prctile',[25 50 75 90]);
hold off
title('plot\_distribution\_prctile');
ylim([-1 5]);


