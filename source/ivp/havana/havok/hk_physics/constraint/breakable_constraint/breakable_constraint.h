#ifndef HK_LOCAL_BREAKABLE_CONSTRAINT_H
#define HK_LOCAL_BREAKABLE_CONSTRAINT_H

class hk_Breakable_BP;
class hk_Local_Constraint_System;

// IVP_EXPORT_PUBLIC

class hk_Breakable_Constraint : public hk_Constraint
{
	public:

		hk_Breakable_Constraint( hk_Environment*,             const hk_Breakable_Constraint_BP* );	
		hk_Breakable_Constraint( hk_Local_Constraint_System*, const hk_Breakable_Constraint_BP* );	
		~hk_Breakable_Constraint();

		void apply_effector_PSI( hk_PSI_Info&, hk_Array<hk_Entity*>* );

		int get_vmq_storage_size() override;

		inline int setup_and_step_constraint( hk_PSI_Info& pi, void *mem, hk_real tau_factor, hk_real damp_factor ) override;
			//: uses the mem as a vmq storage, returns the bytes needed to store its vmq_storage

		void step_constraint( hk_PSI_Info& pi, void* mem, hk_real tau_factor, hk_real damp_factor ) override;
			//: use the mem as a vmq storage setup before

		void init_constraint(const void /*blueprint*/ *) override;

		void write_to_blueprint( hk_Breakable_Constraint_BP * );
		void FireEventIfBroken();

		const char* get_constraint_type() override
		{
			// get the real constraint's type
			return m_real_constraint->get_constraint_type();
		}

		int get_constraint_dof() override
		{
			// get the real constraint's DOF
			return m_real_constraint->get_constraint_dof();
		}

	protected:

		void init_breakable_constraint(const hk_Breakable_Constraint_BP *);

		hk_Constraint* m_real_constraint;

		hk_real m_linear_strength;
		hk_real m_angular_strength;
		hk_real m_bodyMassScale[2];
		bool	m_is_broken;
};

#endif //HK_LOCAL_BREAKABLE_CONSTRAINT_H
