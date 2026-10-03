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
        // A detached scene-owned entity is returned immediately. As<T>() becomes available after startup at the next safe boundary.
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
