# by Herbert Poetzl
filter wobbly_transition (float scale: 0-50, float speed: 0-10, float bertl: 1-5, image input1, image input2)
    mt=t*4;
    nxy=xy/R*scale;
    xyn1=noise([nxy[0],nxy[1],mt*speed*1.5])*sin(t*pi);
    xyn2=noise([nxy[1],nxy[0],(mt+3)*speed]);
    l=clamp(xyn1*sin((mt+xyn2)*2*pi)*bertl+t,0,1);
    lerp(l,input1(xy),input2(xy))
end
