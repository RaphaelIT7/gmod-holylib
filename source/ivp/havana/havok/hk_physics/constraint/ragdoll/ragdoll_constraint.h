#ifndef HK_PHYSICS_RAGDOLL_CONSTRAINT_H
#define HK_PHYSICS_RAGDOLL_CONSTRAINT_H


class hk_Ragdoll_Constraint_BP;
class hk_Local_Constraint_System;
class hk_Ragdoll_Constraint_Work;

// IVP_EXPORT_PUBLIC

class hk_Ragdoll_Constraint : public hk_Constraint
{
	public:

		hk_Ragdoll_Constraint( hk_Environment *,             const hk_Ragdoll_Constraint_BP *, hk_Rigid_Body *a ,hk_Rigid_Body *b );	
		hk_Ragdoll_Constraint( hk_Local_Constraint_System *, const hk_Ragdoll_Constraint_BP *, hk_Rigid_Body *a ,hk_Rigid_Body *b );	

		void apply_effector_PSI( hk_PSI_Info&, hk_Array<hk_Entity*>* );

		int get_vmq_storage_size() override;

		inline int	setup_and_step_constraint( hk_PSI_Info& pi, void *mem, hk_real tau_factor, hk_real damp_factor ) override;
			//: uses the mem as a vmq storage, returns the bytes needed to store its vmq_storage

		void step_constraint( hk_PSI_Info& pi, void *mem, hk_real tau_factor, hk_real damp_factor ) override;
			//: use the mem as a vmq storage setup before

		void init_constraint(const void *) override;
		//: update all parameters of the ragdoll constraint,

		void init_ragdoll_constraint(const hk_Ragdoll_Constraint_BP *, hk_Local_Constraint_System *sys = HK_NULL);
		//: update all parameters of the ragdoll constraint,

		void write_to_blueprint( hk_Ragdoll_Constraint_BP * );

		const char* get_constraint_type() override
		{
			return "ragdoll";
		}

		int get_constraint_dof() override
		{
			return 3;
		}

		inline hk_Transform get_transform(int x) const
		{
			return m_transform_os_ks[x];
		}

		void update_transforms(const hk_Transform& os_ks_0, const hk_Transform& os_ks_1);

		void update_friction(hk_real max_angular_impulse);
	protected:

		void apply_angular_part(hk_PSI_Info& pi, hk_Ragdoll_Constraint_Work&,
				hk_real tau_factor, hk_real strength_factor);

	protected:

		hk_Transform m_transform_os_ks[2];
		hk_Constraint_Limit	m_limits[3];
		hk_Constraint_Limit_BP	m_inputLimits[3];

		hk_real m_strength;
		hk_real m_tau;
		unsigned char              m_axisMap[3];
		bool m_constrainTranslation;

};

#endif /* HK_PHYSICS_RAGDOLL_CONSTRAINT_H */
