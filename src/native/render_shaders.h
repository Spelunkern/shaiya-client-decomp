#pragma once
namespace genesis {
// The row-vector matrices and half-pixel adjustment follow native_render_shaders.h.
// glClipControl(UPPER_LEFT, ZERO_TO_ONE) preserves render-texture coordinates.
inline constexpr char glsl_uniforms[] = R"GLSL(
layout(std140, row_major, binding=0) uniform SceneData {
    mat4 viewProjection, view;
    vec4 lightAmbient, lightDiffuse, lightDirection, camera, fogParameters, fogColor;
    vec4 clipPlane, viewport, pointColor, pointPositionRange, pointAttenuation;
};
layout(std140, row_major, binding=1) uniform ObjectData {
    mat4 world, reflectionProjection;
    vec4 tint, materialDiffuse, materialAmbient, parameters, uvParameters;
    vec4 specularColor, surfaceParameters, emissiveColor;
    mat4 environmentTransform;
    vec4 environmentEye;
    mat4 textureProjection;
    vec4 projectionFlags;
};
layout(std140, row_major, binding=2) uniform SkinData { mat4 bones[256]; };
layout(std140, row_major, binding=3) uniform WaterData {
    mat4 refractionProjection;
    vec4 waterColor, waterParameters, waterOffsets;
};
struct Varyings {
    vec2 uv, lightmapUv;
    vec4 projected;
    float fog;
    vec4 color;
    vec2 reflectionUv;
    float specular;
    vec2 detailUv;
    vec4 textureProjected;
    vec3 worldPosition, normal;
    vec4 refracted;
};
)GLSL";

inline constexpr char glsl_vertex[] = R"GLSL(
layout(location=0) in vec4 inPosition;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec4 inColor;
layout(location=3) in vec2 inUv;
layout(location=4) in vec2 inUv1;
layout(location=5) in vec2 inUv2;
layout(location=6) in float inWeight;
layout(location=7) in uvec4 inBones;
out Varyings vertex;
uniform int vertexPass;
void main() {
    vertex.uv=inUv; vertex.lightmapUv=vec2(0); vertex.projected=vec4(0);
    vertex.fog=1; vertex.color=vec4(1); vertex.reflectionUv=vec2(0);
    vertex.specular=0; vertex.detailUv=vec2(0); vertex.textureProjected=vec4(0);
    vertex.worldPosition=vec3(0); vertex.normal=vec3(0); vertex.refracted=vec4(0);
    gl_ClipDistance[0]=1;
    if(vertexPass==17) {
        vertex.uv=vec2((gl_VertexID<<1)&2,gl_VertexID&2);
        gl_Position=vec4(vertex.uv*vec2(2,-2)+vec2(-1,1),0,1);
        vertex.color=vec4(1,1,1,parameters.w);
        return;
    }
    if(vertexPass==0 || vertexPass==1) {
        float w=inPosition.w==0?1:1/inPosition.w;
        gl_Position=vec4(((inPosition.xy+0.5)*vec2(2,-2)/viewport.xy+vec2(-1,1))*w,inPosition.z*w,w);
        vertex.color=inColor.bgra*tint;
        if(vertexPass==1) vertex.lightmapUv=inUv1;
        return;
    }
    vec3 position=inPosition.xyz, normal=inNormal;
    vec4 color=vec4(1);
    if(vertexPass==6 || vertexPass==9) {
        vec4 p=vec4(position,1)*bones[inBones.x]*inWeight+
               vec4(position,1)*bones[inBones.y]*(1-inWeight);
        normal=(vec4(normal,0)*bones[inBones.x]).xyz*inWeight+
               (vec4(normal,0)*bones[inBones.y]).xyz*(1-inWeight);
        position=p.xyz;
    }
    if(vertexPass==2 || vertexPass==3 || vertexPass==4 || vertexPass==7 ||
       vertexPass==14 || vertexPass==16) color=inColor.bgra;
    vec4 worldPosition=vec4(position,1)*world;
    gl_Position=worldPosition*viewProjection;
    gl_Position.xy+=vec2(1,-1)/viewport.xy*gl_Position.w;
    vertex.color=color*tint;
    if(vertexPass==3 || vertexPass==5 || vertexPass==6 || vertexPass==8 || vertexPass==9) {
        vec3 worldNormal=normalize((vec4(normal,0)*world).xyz);
        vec3 illumination=lightAmbient.rgb*materialAmbient.rgb+
            lightDiffuse.rgb*materialDiffuse.rgb*max(dot(worldNormal,normalize(-lightDirection.xyz)),0);
        if(pointAttenuation.w>0) {
            vec3 toPoint=pointPositionRange.xyz-worldPosition.xyz;
            float distance=length(toPoint);
            float attenuation=pointAttenuation.x+pointAttenuation.y*distance+pointAttenuation.z*distance*distance;
            if(distance<=pointPositionRange.w)
                illumination+=pointColor.rgb*materialDiffuse.rgb*max(dot(worldNormal,toPoint/max(distance,0.000001)),0)/max(attenuation,0.000001);
        }
        vertex.color.rgb=vertex.color.rgb*clamp(illumination,0,1)+tint.rgb*emissiveColor.rgb;
        vertex.color.a*=materialDiffuse.a;
    }
    float depth=(worldPosition*view).z;
    vertex.fog=fogParameters.z>0?clamp((fogParameters.y-depth)/max(0.0001,fogParameters.y-fogParameters.x),0,1):1;
    gl_ClipDistance[0]=any(notEqual(clipPlane,vec4(0)))?dot(worldPosition,clipPlane):1;
    vertex.projected=worldPosition*reflectionProjection;
    if(projectionFlags.x>0) vertex.textureProjected=worldPosition*view*textureProjection;
    if(vertexPass==4 || vertexPass==11 || vertexPass==12) vertex.lightmapUv=inUv1;
    if(vertexPass==12) vertex.detailUv=inUv2;
    if(vertexPass==8 || vertexPass==9) {
        vec3 worldNormal=normalize((vec4(normal,0)*world).xyz);
        vec3 toEye=normalize(camera.xyz-worldPosition.xyz);
        vec3 halfVector=normalize(toEye-normalize(lightDirection.xyz));
        vertex.specular=specularColor.r*pow(max(dot(worldNormal,halfVector),0),max(surfaceParameters.x,0.0001));
        vec3 viewPosition=surfaceParameters.w>0?(worldPosition*environmentTransform).xyz:(worldPosition*view).xyz;
        vec3 viewNormal=normalize(surfaceParameters.w>0?(vec4(worldNormal,0)*environmentTransform).xyz:(vec4(worldNormal,0)*view).xyz);
        vec3 towardEye=surfaceParameters.w>0?normalize(environmentEye.xyz-viewPosition):normalize(-viewPosition);
        vec3 reflected=reflect(-towardEye,viewNormal);
        float divisor=sqrt(max(0.000001,0.5-0.5*reflected.z));
        vertex.reflectionUv=(reflected.xy*(0.25/divisor)+0.5)*vec2(1,-1);
    }
    if(vertexPass==15 || vertexPass==16) vertex.worldPosition=worldPosition.xyz;
    if(vertexPass==15) {
        vertex.normal=normalize((vec4(normal,0)*world).xyz);
        vertex.uv=inUv+waterOffsets.xy; vertex.lightmapUv=inUv1+waterOffsets.zw;
        vertex.refracted=worldPosition*refractionProjection;
    }
    if(vertexPass==16) vertex.uv=inUv+uvParameters.xy;
}
)GLSL";

inline constexpr char glsl_fragment[] = R"GLSL(
in Varyings vertex;
layout(location=0) out vec4 outColor;
layout(binding=0) uniform sampler2D image;
layout(binding=1) uniform sampler2D secondary;
layout(binding=2) uniform sampler2D detailImage;
layout(binding=3) uniform sampler2D secondNormal;
uniform int pixelPass;
void main() {
    vec2 uv=projectionFlags.x>0?vertex.textureProjected.xy/vertex.textureProjected.w:vertex.uv;
    vec4 color;
    bool fogged=true;
    if(pixelPass==0 || pixelPass==1 || pixelPass==19) {
        color=texture(image,vertex.uv)*vertex.color;
        if(pixelPass==1) color.a*=texture(secondary,vertex.uv).a;
        if(pixelPass==19) {
            vec4 mask=texture(secondary,vertex.lightmapUv);
            color.a*=mask.a;
            if(surfaceParameters.z>0) color.rgb=mask.rgb;
        }
        fogged=false;
    } else if(pixelPass==2 || pixelPass==3) {
        color=texture(image,uv)*vertex.color;
        if(pixelPass==3) color.rgb*=texture(secondary,vertex.lightmapUv).rgb;
    } else if(pixelPass==10) {
        vec4 base=texture(image,uv);
        color=vec4(base.rgb*vertex.color.rgb+texture(secondary,vertex.reflectionUv).rgb*base.a*vertex.specular,tint.a);
    } else if(pixelPass==13 || pixelPass==14 || (pixelPass>=15 && pixelPass<=18)) {
        vec4 base=texture(image,uv);
        vec3 rgb=surfaceParameters.y>0?base.rgb:vec3(1);
        vec3 second=texture(secondary,vertex.lightmapUv).rgb;
        if(pixelPass==15 || pixelPass==17) rgb=second;
        else if(pixelPass==16 || pixelPass==18) rgb=clamp(rgb+second-0.5,0,1);
        else rgb*=second;
        if(pixelPass==13 || pixelPass==17 || pixelPass==18) rgb*=texture(detailImage,vertex.detailUv).rgb;
        color=vec4(rgb*vertex.color.rgb,base.a*vertex.color.a);
    } else if(pixelPass==4) {
        vec2 reflectedUv=vertex.projected.xy/vertex.projected.w;
        reflectedUv+=texture(image,vertex.uv+uvParameters.xy).rg*parameters.y;
        color=vec4(mix(vertex.color.rgb,texture(secondary,reflectedUv).rgb,parameters.z),vertex.color.a);
    } else if(pixelPass==11 || pixelPass==12) {
        vec2 projected=vertex.projected.xy/vertex.projected.w;
        if(any(lessThan(projected,vec2(0))) || any(greaterThan(projected,vec2(1)))) discard;
        color=pixelPass==11?vec4(0,0,0,texture(image,projected).a*parameters.z):texture(image,projected);
    } else if(pixelPass==20) {
        vec3 first=texture(image,vertex.uv).xyz, second=texture(secondNormal,vertex.lightmapUv).xyz;
        if(waterParameters.w<0.5) { first=first*2-1; second=second*2-1; }
        vec3 normal=normalize(vertex.normal+vec3(first.x+second.x,0,first.y+second.y)*0.5);
        vec2 distortion=(first.xy+second.xy)*waterParameters.x*0.5;
        vec2 reflectedUv=vertex.projected.xy/vertex.projected.w+distortion;
        vec2 refractedUv=vertex.refracted.xy/vertex.refracted.w-distortion;
        float fresnel=waterParameters.y+(1-waterParameters.y)*pow(1-clamp(dot(normal,normalize(camera.xyz-vertex.worldPosition)),0,1),5);
        vec3 reflected=texture(secondary,reflectedUv).rgb;
        vec3 refracted=texture(detailImage,refractedUv).rgb*waterColor.rgb;
        color=vec4(mix(refracted,reflected,clamp(fresnel,0,1))*tint.rgb,waterParameters.z*tint.a);
    } else if(pixelPass==21) {
        vec3 sampled=texture(image,vertex.uv).xyz*2-1;
        vec3 normal=normalize((vec4(sampled.x,sampled.z,sampled.y,0)*world).xyz);
        vec3 eye=(vec4(0,0,0,1)*environmentTransform).xyz;
        vec3 reflected=reflect(-normalize(eye-vertex.worldPosition),normal);
        float divisor=sqrt(max(0.000001,0.5-0.5*reflected.z));
        vec2 environmentUv=(reflected.xy*(0.25/divisor)+0.5)*vec2(1,-1);
        float illumination=0.25+0.75*clamp(dot(normal,normalize(environmentEye.xyz-vertex.worldPosition)),0,1);
        color=vec4(texture(secondary,environmentUv).rgb*illumination*vertex.color.rgb,vertex.color.a);
    } else {
        fogged=false;
        vec3 rgb=vec3(0);
        if(pixelPass==5) rgb=texture(image,vertex.uv).rgb;
        if(pixelPass==9) rgb=pow(clamp(texture(image,vertex.uv).rgb,0,1),parameters.yyy);
        if(pixelPass==6) {
            for(int x=0;x<2;++x) for(int y=0;y<4;++y) {
                vec2 offset=vec2(x==0?-1:3,y==0?-1:(y==1?-3:(y==2?1:3)));
                rgb+=texture(image,vertex.uv+offset*uvParameters.zw).rgb;
            }
            rgb/=6;
        }
        if(pixelPass==7 || pixelPass==8) {
            const float weights[7]=float[](1./64,6./64,15./64,20./64,15./64,6./64,1./64);
            vec2 axis=pixelPass==7?vec2(1,0):vec2(0,1);
            for(int i=0;i<7;++i) rgb+=texture(image,vertex.uv+(i-3)*axis*uvParameters.zw).rgb*weights[i];
        }
        outColor=vec4(rgb,pixelPass==9?1:parameters.w); return;
    }
    if(color.a<parameters.x) discard;
    if(fogged) color.rgb=mix(fogColor.rgb,color.rgb,vertex.fog);
    outColor=color;
}
)GLSL";
} // namespace genesis
