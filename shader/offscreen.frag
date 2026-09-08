#version 330 core

uniform uint pickId;

in vec3 facenormal;
out uint color;

void main(){
    color = pickId;
}