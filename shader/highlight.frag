#version 330 core

uniform vec4 baseColor;

in vec3 facenormal;
out vec4 color;

void main(){
    color = baseColor;
}