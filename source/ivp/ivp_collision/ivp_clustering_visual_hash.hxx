// Copyright (C) Ipion Software GmbH 2000. All rights reserved.

// IVP_EXPORT_PROTECTED

/********************************************************************************
 * Class:	IVP_Clustering_Visualizer_Object_Hash
 * Description:	hash table to store simple IVP_Real_Object pointers.
 *		Nothing fancy.
 ********************************************************************************/

class IVP_Clustering_Visualizer_Object_Hash : protected IVP_VHash {
protected:
    IVP_BOOL          compare     (const void *elem0, const void *elem1) const override;
    int               obj_to_index(IVP_Real_Object *obj);

public:
    void              add         (IVP_Real_Object *obj);
    IVP_Real_Object * remove      (IVP_Real_Object *obj);
    IVP_Real_Object * find        (IVP_Real_Object *obj);

    IVP_Clustering_Visualizer_Object_Hash(int create_size);
    ~IVP_Clustering_Visualizer_Object_Hash();
};

