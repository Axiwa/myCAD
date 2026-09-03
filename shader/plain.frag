#version 330 core

uniform vec4 baseColor;

in vec3 facenormal;
out vec4 color;

void main(){
    //color = vec4((normalize(facenormal)*0.5f+0.5f), 1.0);
    color = baseColor;
}