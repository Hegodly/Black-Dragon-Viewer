/* SMAA T2x temporal resolve fragment stage. */
/*[EXTRA_CODE_HERE]*/
out vec4 frag_color;
in vec2 vary_texcoord0;
uniform sampler2D currentColorTex;
uniform sampler2D previousColorTex;
#if SMAA_REPROJECTION
uniform sampler2D velocityTex;
#endif
#define float4 vec4
#define float2 vec2
#define SMAATexture2D(tex) sampler2D tex
float4 SMAAResolvePS(float2 texcoord,
                     SMAATexture2D(currentColorTex),
                     SMAATexture2D(previousColorTex)
                     #if SMAA_REPROJECTION
                     , SMAATexture2D(velocityTex)
                     #endif
                     );
void main()
{
    frag_color=SMAAResolvePS(vary_texcoord0,currentColorTex,previousColorTex
                            #if SMAA_REPROJECTION
                            ,velocityTex
                            #endif
                            );
}
