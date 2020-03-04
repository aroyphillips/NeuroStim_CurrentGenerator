clear

% trainPrd = 10;
% trainDur = 100;
% trainWid = 5;
% trainDly = 10;
% trainAmp = 2;
% A2V = 0.5;
% 
% 
% 
% 
% train_y = zeros(1,181);
% single_pulse = zeros(1,trainPrd+1); 
% single_pulse(1:trainWid+1) = trainAmp/A2V;
% all_pulses = repmat(single_pulse, 1, ceil(trainDur/trainPrd));
% all_pulses = all_pulses(1:trainDur+1);
% 
% train_inds = [(trainDly+1):(trainDly+trainDur+1)];
% train_y(train_inds) = all_pulses;    
% 
% plot(train_y);

triAmp = 3;
triDly = 50;
triPk = 100;
triDur = 70;
triSlopeUp = triAmp/(triPk-triDly);
triSlopeDown = triAmp/(triPk-triDly-triDur);

triPulseUp = ([triDly:triPk]-triDly)*triSlopeUp;
triPulseDown = (0:(triDur+triDly-triPk))*triSlopeDown+triAmp;
triPulse = [triPulseUp, triPulseDown];

plot(triPulse);