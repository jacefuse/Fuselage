#version 450

layout(location = 0) in flat float fragCharacterId;
layout(location = 1) in vec4 fragColor;
layout(location = 2) in vec2 fragLocalCoord;

layout(binding = 0) uniform sampler2D atlasTexture;

layout(location = 0) out vec4 outColor;

void main() {
    uint char_x = uint(fragCharacterId) % 16u;
    uint char_y = uint(fragCharacterId) / 16u;
    
    // Flip the Y coordinate to fix upside-down issue
    vec2 flippedCoord = vec2(fragLocalCoord.x, 1.0 - fragLocalCoord.y);
    
    vec2 atlasCoord = vec2(
        (float(char_x) + flippedCoord.x) / 16.0,
        (float(char_y) + flippedCoord.y) / 16.0
    );
    
    // Sample atlas and apply color tinting
    vec4 atlasColor = texture(atlasTexture, atlasCoord);
    outColor = vec4(fragColor.rgb * atlasColor.rgb, atlasColor.a * fragColor.a);
}