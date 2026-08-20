#version 330 core

in vec3 facenormal;
out vec4 color;

void main(){
    color = vec4((normalize(facenormal)*0.5+0.5), 1.0);
}