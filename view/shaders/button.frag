#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D texSampler;
// dimFactor: 1.0 = кнопка активна (полная яркость), 0.4 = кнопка выключена (затемнена)
uniform float dimFactor;
// hoverGlow: 1.0 = курсор над кнопкой (подсветка +0.25 RGB), 0.0 = без подсветки
uniform float hoverGlow;
void main() {
    vec4 texColor = texture(texSampler, TexCoord);
    vec3 rgb = texColor.rgb * dimFactor;
    rgb = mix(rgb, rgb + vec3(0.25), hoverGlow);
    FragColor = vec4(rgb, texColor.a);
}
