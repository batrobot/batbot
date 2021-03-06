% func_map_pid_to_servo: map controller input to drv8835 commands
% u: control action, 4-by-1 vec
%   u(1): right forelimb
%   u(2): left forelimb
%   u(3): right leg
%   u(4): left leg
% error: pos error, 4-by-1 vec
%   error(1): right forelimb
%   error(2): left forelimb
%   error(3): right leg
%   error(4): left leg
% u_drv: drv8835 control command, 8-by-1 vec
%   u_drv(1): right forelimb
%   u_drv(2): left forelimb
%   u_drv(3): right leg
%   u_drv(4): left leg
% by ALireza Ramezani, 8-31-2015, Champaign, IL
function u_drv = func_map_pid_to_servo(u,error)
%#codegen
u_drv = zeros(8,1);
H = zeros(8,4);

% used for saturating control command to drv8835
global PID_SATURATION_THRESHOLD;
global PID_TRACKING_PRECISION_THRESHOLD;
global ERR_INTEGRALE;

% map
H = [func_heaviside(u(1)),0,0,0;...
     func_heaviside(-u(1)),0,0,0;...
     0,func_heaviside(u(2)),0,0;...
     0,func_heaviside(-u(2)),0,0;...
     0,0,func_heaviside(u(3)),0;...
     0,0,func_heaviside(-u(3)),0;...
     0,0,0,func_heaviside(u(4));...
     0,0,0,func_heaviside(-u(4))];

% saturate u
for i=1:4
    if u(i)> PID_SATURATION_THRESHOLD
        u(i) = PID_SATURATION_THRESHOLD;
    end
    
    if u(i)< -PID_SATURATION_THRESHOLD
        u(i) = -PID_SATURATION_THRESHOLD;
    end
end


% if position err is too small: set ctrl effort to zero
for i=1:4
    if abs(error(i))<PID_TRACKING_PRECISION_THRESHOLD
        u(i) = 0;
        ERR_INTEGRALE(i) = 0;
    end
end


u_drv = H*abs(u);

end
% end of code