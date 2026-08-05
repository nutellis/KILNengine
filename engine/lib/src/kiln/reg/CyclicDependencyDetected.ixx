export module kiln.reg.CyclicDependencyDetected;

import kiln.util.contracts;

namespace kiln::reg {

export class CyclicDependencyDetected : public util::PreconditionViolation   //
{
    using PreconditionViolation::PreconditionViolation;
};

}   // namespace kiln::reg
