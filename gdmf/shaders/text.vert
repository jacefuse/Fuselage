#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in float inCharacterId;    // Changed from uint to float
layout(location = 2) in vec4 inColor;

layout(location = 0) out flat float fragCharacterId;  // Changed from uint to float
layout(location = 1) out vec4 fragColor;
layout(location = 2) out vec2 fragLocalCoord;

void main() {
    fragCharacterId = inCharacterId;
    fragColor = inColor;
    
    int vertexInQuad = gl_VertexIndex % 6;
    
    switch(vertexInQuad) {
        case 0: fragLocalCoord = vec2(0.0, 0.0); break;
        case 1: fragLocalCoord = vec2(0.0, 1.0); break;
        case 2: fragLocalCoord = vec2(1.0, 0.0); break;
        case 3: fragLocalCoord = vec2(0.0, 1.0); break;
        case 4: fragLocalCoord = vec2(1.0, 1.0); break;
        case 5: fragLocalCoord = vec2(1.0, 0.0); break;
    }
    
    gl_Position = vec4(inPosition, 0.0, 1.0);
}