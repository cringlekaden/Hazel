using System;
using System.Runtime.CompilerServices;

namespace Hazel
{
	public class Entity
	{
		protected Entity() { ID = 0; }

		internal Entity(ulong id)
		{
			ID = id; SceneIdentity = InternalCalls.Entity_GetSceneIdentity();
		}

		public readonly ulong ID;
        private readonly ulong SceneIdentity;
        public bool IsValid { get { return InternalCalls.Entity_IsValid(ID, SceneIdentity); } }
        internal ulong CheckedID { get { if (!IsValid) throw new InvalidOperationException("Entity is destroyed or its scene has retired"); return ID; } }
        // A detached scene-owned entity and its physics are usable immediately. As<T>() becomes available after startup at the next safe boundary.
        // Position-only placement preserves the asset's authored rotation and scale.
        public static Entity Instantiate(Prefab prefab, Vector3 position) {
            if (prefab == null || !prefab.IsAssigned) throw new ArgumentException("Select a prefab asset in the Inspector");
            Vector3 rotation = Vector3.Zero, scale = new Vector3(1,1,1);
            return new Entity(InternalCalls.Entity_Instantiate(prefab.Path, ref position, ref rotation, ref scale, false));
        }
        public static Entity Instantiate(Prefab prefab, Vector3 position, Vector3 rotation, Vector3 scale) {
            if (prefab == null || !prefab.IsAssigned) throw new ArgumentException("Select a prefab asset in the Inspector");
            return new Entity(InternalCalls.Entity_Instantiate(prefab.Path, ref position, ref rotation, ref scale, true));
        }
        // Invalid immediately, OnDestroy/physics cleanup at a safe callback boundary. Repeated Destroy is harmless.
        public void Destroy() { InternalCalls.Entity_Destroy(ID, SceneIdentity); }
        public Entity Parent { get { ulong id=InternalCalls.Hierarchy_GetParent(CheckedID); return id==0?null:new Entity(id); } }
        public Entity[] Children { get { var ids=InternalCalls.Hierarchy_GetChildren(CheckedID); var result=new Entity[ids.Length]; for(int i=0;i<ids.Length;++i)result[i]=new Entity(ids[i]);return result; } }
        // Queued during runtime. Check Status/Reason after a lifecycle boundary; rejected requests do not change the graph.
        public ParentingRequest SetParent(Entity parent, ParentingMode mode = ParentingMode.KeepWorld) {
            ulong parentID=0;
            if(parent!=null) { if(parent.SceneIdentity!=SceneIdentity)throw new InvalidOperationException("Cannot parent across scenes");parentID=parent.CheckedID; }
            return new ParentingRequest(SceneIdentity,InternalCalls.Hierarchy_SetParent(CheckedID,parentID,(int)mode));
        }
        public ParentingRequest Detach(ParentingMode mode = ParentingMode.KeepWorld) => SetParent(null,mode);
        public Vector3 LocalTranslation { get { InternalCalls.Transform_GetLocal(CheckedID,0,out Vector3 result);return result; } set { InternalCalls.Transform_SetLocal(CheckedID,0,ref value); } }
        // Radians, like authored engine rotation.
        public Vector3 LocalRotation { get { InternalCalls.Transform_GetLocal(CheckedID,1,out Vector3 result);return result; } set { InternalCalls.Transform_SetLocal(CheckedID,1,ref value); } }
        public Vector3 LocalScale { get { InternalCalls.Transform_GetLocal(CheckedID,2,out Vector3 result);return result; } set { InternalCalls.Transform_SetLocal(CheckedID,2,ref value); } }
        public Matrix4 WorldMatrix { get { InternalCalls.Transform_GetWorldMatrix(CheckedID,out Matrix4 result);return result; } }


		// Legacy wrappers remain world-space; Scale rejects sheared/nonrepresentable world TRS.
        public Vector3 Scale {
            get { InternalCalls.TransformComponent_GetScale(CheckedID, out Vector3 result); return result; }
            set { InternalCalls.TransformComponent_SetScale(CheckedID, ref value); }
        }
        public Vector3 Translation
		{
			get
			{
				InternalCalls.TransformComponent_GetTranslation(CheckedID, out Vector3 result);
				return result;
			}
			set
			{
				InternalCalls.TransformComponent_SetTranslation(CheckedID, ref value);
			}
		}

		public bool HasComponent<T>() where T : Component, new()
		{
			Type componentType = typeof(T);
			return InternalCalls.Entity_HasComponent(CheckedID, componentType);
		}

		public T GetComponent<T>() where T : Component, new()
		{
			if (!HasComponent<T>())
				return null;

			T component = new T() { Entity = this };
			return component;
		}

		public Entity FindEntityByName(string name)
		{
			if (!IsValid) throw new InvalidOperationException("Entity scene has retired or this is not a runtime callback");
            ulong entityID = InternalCalls.Entity_FindEntityByName(name);
			if (entityID == 0)
				return null;

			return new Entity(entityID);
		}

		public T As<T>() where T : Entity, new()
		{
			object instance = InternalCalls.GetScriptInstance(CheckedID);
			return instance as T;
		}

	}

}
