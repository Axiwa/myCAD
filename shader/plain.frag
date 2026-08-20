#version 330 core

in vec3 facenormal;
out vec4 color;

void main(){
    color = vec4(abs(normalize(facenormal)), 1.0);
}