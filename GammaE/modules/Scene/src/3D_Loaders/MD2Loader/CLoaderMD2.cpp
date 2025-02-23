// ----------------------------------------------------------------------------
/*! \class
 *  \brief
 *  \author David M&aacute;rquez de la Cruz
 *  \version 1.5
 *  \date 1999-2009
 *  \par Copyright (c) 1999 David M&aacute;rquez de la Cruz
 *  \par GammaE License
 */
// ----------------------------------------------------------------------------
// CLoaderMD2
// ----------------------------------------------------------------------------
#include "3D_Loaders\MD2Loader\CLoaderMD2.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "GammaE_Mem.h"
// ----------------------------------------------------------------------------
CLoaderMD2::CLoaderMD2()
    : Stream(NULL), StreamSize(0), MD2Header(NULL), Frame(NULL), Triangles(NULL), Vertexs(NULL), TexCoords(NULL)
{
}
// ----------------------------------------------------------------------------
CLoaderMD2::~CLoaderMD2()
{
    if ( Stream ) MEMFree(Stream);
}
// ----------------------------------------------------------------------------
int CLoaderMD2::ReadFile(const CGString& _sFilename)
{
    // ----------------------------------------------------------------------------
    // Read file contents
    // ----------------------------------------------------------------------------
    CGFile oFile;
    if (!oFile.bOpen(_sFilename, EFileOpenMode::FOM_READ))
    {
        return 0;
    }

    StreamSize = oFile.uiLength();

    Stream = (pointer)MEMAlloc(StreamSize);
    if ( !Stream )
    {
        oFile.Close();
        return(0);

    }
    
    oFile.uiReadData((pointer)Stream, StreamSize);
    
    oFile.Close();
    return(1);
}
// ----------------------------------------------------------------------------
void CLoaderMD2::SetupFrame (int iFrame)
{
    Frame = (frame_t*)&Stream[MD2Header->offsetFrames + iFrame * MD2Header->frameSize ];
    Vertexs = (triangleVertex_t*)&Frame->vertices;
}
// ----------------------------------------------------------------------------
void CLoaderMD2::ParseHeader ()
{
    MD2Header = (model_t*)Stream;

    SetupFrame(0);

    Triangles = (triangle_t*)&Stream[ MD2Header->offsetTriangles ];
    TexCoords = (textureCoordinate_t*)&Stream[ MD2Header->offsetTexCoords ];
}
// ----------------------------------------------------------------------------
CGSceneNode* CLoaderMD2::ParseChunks ()
{
    SCNUt_TriScene* MD2Mesh;

    SCNUt_MeshCompacter MCompacter;

    CGShader* Skin;

    CGSceneAnimMesh* AnimMesh;
    CGSceneLeaf* LeafObj;
    CGMesh* Mesh;
    // Parse model header
    ParseHeader();

    if ( MD2Header->magic != ( 'I' + ('D' << 8) + ('P' << 16) + ('2' << 24) ) ) return(NULL);
    if ( MD2Header->version != 8 ) return(NULL);

    // ----------------------------------------
    // Get mesh info
    // ----------------------------------------
    MD2Mesh = ParseModel();

    // Generate a legal mesh
    Mesh = MCompacter.poCompactMesh(*MD2Mesh);

    // free auxiliary resources
    mDel MD2Mesh;

    // ----------------------------------------
    // Get material info
    // ----------------------------------------
    // Get the material
    Skin = ParseMaterial ();

    // ----------------------------------------
    // Setup the model & material & frames
    // ----------------------------------------
    AnimMesh = mNew CGSceneAnimMesh();
    AnimMesh->SetShader(Skin);
    AnimMesh->Setup(Mesh, MD2Header->numFrames, Mesh->uiGetNumVXs());

    // Parse model frames
    ParseFrameSet(*AnimMesh,*Mesh);

    glbAnimMesh_NormalGenerator.Generate(AnimMesh);

    return(AnimMesh);
}
// ----------------------------------------------------------------------------
SCNUt_TriScene* CLoaderMD2::ParseModel ()
{
    CGVect3 VX;
    int cTri;
    uint cVert;
    uint uiVPos;
    uint uiTPos;

    SCNUt_TriScene* MD2Mesh = mNew SCNUt_TriScene();

    SetupFrame(0);

    int iVert;
    MD2Mesh->Init(MD2Header->numTriangles);
    for ( cTri = 0; cTri < MD2Header->numTriangles; cTri++ )
    {
        for ( cVert = 0; cVert < 3; cVert++ )
        {
            // Reverse winding while reading
            iVert = 2 - cVert;

            uiVPos = Triangles[cTri].vertexIndices[iVert];
            uiTPos = Triangles[cTri].textureIndices[iVert];

            MD2Mesh->Tris[cTri].VXs[cVert].Set(
                Vertexs[uiVPos].vertex[0] * Frame->scale[0] + Frame->translate[0],
                Vertexs[uiVPos].vertex[1] * Frame->scale[1] + Frame->translate[1],
                Vertexs[uiVPos].vertex[2] * Frame->scale[2] + Frame->translate[2]);

            MD2Mesh->Tris[cTri].UVs[cVert].Set(
                (float)TexCoords[uiTPos].s / (float)MD2Header->skinWidth,
                (float)TexCoords[uiTPos].t / (float)MD2Header->skinHeight);

            MD2Mesh->Tris[cTri].VCs[cVert].Set(1,1,1,1);
        }
    }

    return(MD2Mesh);
}
// ----------------------------------------------------------------------------
CGShader* CLoaderMD2::ParseMaterial ()
{
    char* SkinFile = (char*)&Stream[MD2Header->offsetSkins];

    return( CGShaderWH::I()->poCreateShader(SkinFile) );
}
// ----------------------------------------------------------------------------
unsigned short* CLoaderMD2::GetVertexConversionTable (CGMesh &Mesh)
{
    CGVect3 NewVX;
    int cVert,cMD2Vert;
    unsigned short* NewIdxs = (unsigned short*)MEMAlloc(Mesh.uiGetNumVXs() * sizeof(unsigned short));

    // Obtener los nuevos indices en funcion de los antiguos
    for ( cVert = 0; cVert < Mesh.uiGetNumVXs(); cVert++ )
    {
        // Buscar el vertice MD2 al que corresponde el v�rtice actual
        cMD2Vert = 0;
        do {
            NewVX.Set(
                Vertexs[cMD2Vert].vertex[0] * Frame->scale[0] + Frame->translate[0],
                Vertexs[cMD2Vert].vertex[1] * Frame->scale[1] + Frame->translate[1],
                Vertexs[cMD2Vert].vertex[2] * Frame->scale[2] + Frame->translate[2]
                );

            cMD2Vert++;
        } while ( ( !NewVX.bEqual(Mesh.m_poVX[cVert]) ) && (cMD2Vert < MD2Header->numVertices) );

        NewIdxs[cVert] = cMD2Vert - 1;
    }

    return(NewIdxs);
    /*
       unsigned char   ucX,ucY,ucZ;

       do {
        Vertexs = (triangleVertex_t *)&Frame->vertices + cOldVert;
        ucX = ((Mesh.VXs[cVert].x - Frame->translate[0]) / Frame->scale[0]);
        ucY = ((Mesh.VXs[cVert].y - Frame->translate[1]) / Frame->scale[1]);
        ucZ = ((Mesh.VXs[cVert].z - Frame->translate[2]) / Frame->scale[2]);
       }while (! ((ucX == vertex[0]) && (ucY == vertex[1]) && (ucZ == vertex[2]))
            &&
            (cOldVert < MD2Header->numVertices));
     */
}
// ----------------------------------------------------------------------------
void CLoaderMD2::ParseFrameSet (CGSceneAnimMesh &AnimMesh, CGMesh &Mesh)
{
    int cVert,cFrame;
    unsigned short* NewIdxs = GetVertexConversionTable( Mesh );

    CGVect3 NewVX;
    CGVect3* pVX = AnimMesh.poGetVertices();

    // Parse every frame
    for ( cFrame = 0; cFrame < MD2Header->numFrames; cFrame++ )
    {
        SetupFrame(cFrame);
        Frame = (frame_t*)&Stream[ MD2Header->offsetFrames + cFrame * MD2Header->frameSize];

        // Convert from MD2 frames to engine frames
        for ( cVert = 0; cVert < Mesh.uiGetNumVXs(); cVert++ )
        {
            NewVX.Set(
                (float)(Vertexs[ NewIdxs[cVert] ].vertex[0]) * Frame->scale[0] + Frame->translate[0],
                (float)(Vertexs[ NewIdxs[cVert] ].vertex[1]) * Frame->scale[1] + Frame->translate[1],
                (float)(Vertexs[ NewIdxs[cVert] ].vertex[2]) * Frame->scale[2] + Frame->translate[2]
            );

            pVX->Assign(NewVX);
            pVX++;
        }
    }

    MEMFree ( NewIdxs );
}
// ----------------------------------------------------------------------------
CGSceneNode* CLoaderMD2::poLoad(const CGString& _sFilename)
{
    CGSceneNode* Obj = NULL;

    if ( !ReadFile(_sFilename) ) return(NULL);

    // ----------------------------------------------------------------------------

    Obj = ParseChunks();

    return(Obj);
}
// ----------------------------------------------------------------------------
int CLoaderMD2::GetNumSkins ()
{
    return (MD2Header->numSkins);
}
// ----------------------------------------------------------------------------
char* CLoaderMD2::GetSkin (int iSkin)
{
    return ((char*)&Stream[MD2Header->offsetSkins + iSkin * 64]);
}
// ----------------------------------------------------------------------------
int CLoaderMD2::GetNumFrames ()
{
    return (MD2Header->numFrames);
}
// ----------------------------------------------------------------------------
CGSceneAnimActionSet* CLoaderMD2::pLoadQ2Player(const CGString& _sFilename)
{
    CGSceneAnimActionSet* pQ2Player = NULL;
    CGSceneAnimMesh* pQ2Model = NULL;

    pQ2Model = (CGSceneAnimMesh*)poLoad(_sFilename);

    if ( pQ2Model )
    {
        pQ2Player = mNew CGSceneAnimActionSet();
        pQ2Player->uiAddAction("full", 0,MD2Header->numFrames,40,true);

        pQ2Player->uiAddAction("stand", 0, 39, 10.0f, true);    // stand
        pQ2Player->uiAddAction("run", 40,45, 0.5f,true );       // run
        pQ2Player->uiAddAction("attack", 46,53, 2.0f,true );    // attack
        pQ2Player->uiAddAction("pain1", 54,57, 2.0f,false);     // pain 1
        pQ2Player->uiAddAction("pain2", 58,61, 2.0f,false);     // pain 2
        pQ2Player->uiAddAction("pain3", 62,65, 2.0f,false);     // pain 3
        pQ2Player->uiAddAction("jump", 66,71, 2.0f,false);      // jump
        pQ2Player->uiAddAction("flip", 71,84, 2.0f,false);      // flip

        pQ2Player->SetAnimObject(pQ2Model);
    }

    return (pQ2Player);
}
// ----------------------------------------------------------------------------
