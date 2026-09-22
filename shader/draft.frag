#version 330 core

#define PI 3.14159265359
#define INV_PI 0.3183099

uniform vec4 baseColor;

uniform int analyzeDraft;
uniform vec3 pullDir;
uniform float draftThreshold;


in vec3 facenormal;
out vec4 color;

void main(){
    if (analyzeDraft != 0){
        float angle = 180 * INV_PI * asin(clamp(dot(facenormal, pullDir), -1, 1));
        if (angle >= draftThreshold){
            color = vec4(0, 1, 0, 1);
        }
        else if (angle < draftThreshold && angle >= 0) {
            color = vec4(1, 1, 0, 1);
        }
        else if (angle < 0 && angle >= -draftThreshold){
            color = vec4(1, 0.5, 0, 1);
        }
        else {
            color = vec4(1, 0, 0, 1);
        }
    }
    else{
        color = baseColor;
    }
}