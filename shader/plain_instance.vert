#version 330 core

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 3) in mat4 model;

uniform mat4 view;
uniform mat4 proj;

out vec3 facenormal;

void main(){
    gl_Position = proj * view * model * vec4(pos, 1.0f);
    facenormal = vec3(inverse(transpose(model)) * vec4(normal, 0.0f));
}