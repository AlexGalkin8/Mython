#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace ast
{

    using runtime::Closure;
    using runtime::Context;
    using runtime::ObjectHolder;


    namespace
    {
        const string ADD_METHOD = "__add__"s;
        const string INIT_METHOD = "__init__"s;
    }  // namespace



    /***************   VariableValue   ***************/

    VariableValue::VariableValue(const std::string& var_name)
        : name_(var_name)
    {
    }


    VariableValue::VariableValue(std::vector<std::string> dotted_ids)
        : dotted_ids_(std::move(dotted_ids))
    {
    }


    ObjectHolder VariableValue::Execute(Closure& closure, Context& context)
    {
        if (!name_.empty() && closure.count(name_))
        {
            return closure.at(name_); // Вычисляем значение переменной
        }
        else if (!dotted_ids_.empty() && closure.count(dotted_ids_.at(0))) 
        {
            // Если цепочка вызовов полей объектов не пуста и начальный обьект в цепи присутствует

            // Запрашиваем обьект с первым именем в цепочке и начальным полем обьетов closure
            ObjectHolder out = VariableValue(dotted_ids_.at(0)).Execute(closure, context);
            Closure c(closure);
            // Итерируемся по цепочке полей обьектов, начиная со второго обьекта
            for (size_t i = 1; i < dotted_ids_.size(); i++)
            {
                // Обьектом, у которого есть поля являются только экземпляры класса
                runtime::ClassInstance* cls_instance = dynamic_cast<runtime::ClassInstance*>(out.Get());
                // Запрашиваем обьект с i именем в цепочке и в качестве полей обьектов передаём поля экземпляра класса
                c.insert(cls_instance->Fields().begin(), cls_instance->Fields().end());

                out = VariableValue(dotted_ids_.at(i)).Execute(c, context);
            }
            return out;
        }
        else
        {
            throw std::runtime_error("VariableValue::Execute: There is no value with the given name"s);
        }
    }



    /***************   Assignment   ***************/

    Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv)
        : name_(std::move(var)), rv_(std::move(rv))
    {
    }


    ObjectHolder Assignment::Execute(Closure& closure, Context& context)
    {
        if (!rv_)
            throw std::runtime_error("Print::Execute: Null pointer");

        closure[name_] = rv_->Execute(closure, context); // Присвиваем имя переменной. Если данной переменной нет в closure, то создаём
        return closure.at(name_);
    }



    /***************   Print   ***************/

    Print::Print(unique_ptr<Statement> argument)
    {
        args_.push_back(std::move(argument));
    }


    Print::Print(vector<unique_ptr<Statement>> args)
        : args_(std::move(args))
    {
    }


    unique_ptr<Print> Print::Variable(const std::string& name)
    {
        return std::make_unique<Print>(std::move(std::make_unique<VariableValue>(name)));
    }


    ObjectHolder Print::Execute(Closure& closure, Context& context)
    {
        bool b = false;
        for (std::unique_ptr<Statement>& arg : args_)
        {
            if (!arg)
                throw std::runtime_error("Print::Execute: Null pointer");

            if (b)
                context.GetOutputStream() << " "s;

            ObjectHolder obj = arg->Execute(closure, context);
            if (obj)
                obj->Print(context.GetOutputStream(), context);
            else
                context.GetOutputStream() << "None"s;

            b = true;
        }

        context.GetOutputStream() << std::endl;

        return {};
    }



    /***************   MethodCall   ***************/

    MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
        std::vector<std::unique_ptr<Statement>> args)
        : object_(std::move(object))
        , method_name_(std::move(method))
        , method_args_(std::move(args))
    {
    }


    ObjectHolder MethodCall::Execute(Closure& closure, Context& context)
    {
        if (!object_)
            throw std::runtime_error("MethodCall::Execute: Null pointer");

        // Запрашиваем интерфейс класса, который передан в переменной-объекте
        runtime::ClassInstance* cls_instance =
            dynamic_cast<runtime::ClassInstance*>(object_->Execute(closure, context).Get());

        // Создаём массив аргументов-обьектов
        std::vector<runtime::ObjectHolder> method_args;

        // Получаем обьекты из переданных Statement'ов
        for (auto& arg : method_args_)
        {
            if (!arg)
                throw std::runtime_error("MethodCall::Execute: Null pointer");
            method_args.push_back(arg->Execute(closure, context));
        }
        // Вызываем метод
        return cls_instance->Call(method_name_, method_args, context);
    }



    /***************   Stringify   ***************/

    ObjectHolder Stringify::Execute(Closure& closure, Context& context)
    {
        if (!argument_)
            throw std::runtime_error("Stringify::Execute: Null pointer");

        std::stringstream out;
        // Получаем обьект из Statement'а
        auto arg = argument_->Execute(closure, context);

        if (arg) // Если имеется обьект
            arg->Print(out, context);
        else     // Если обьект отсутствует, то это None
            out << "None"s;

        return ObjectHolder::Own(runtime::String(out.str()));
    }



    /***************   Add   ***************/

    ObjectHolder Add::Execute(Closure& closure, Context& context)
    {
        if (!lhs_ || !rhs_)
            throw std::runtime_error("Add::Execute: Null pointer");

        ObjectHolder lhs = lhs_->Execute(closure, context);
        ObjectHolder rhs = rhs_->Execute(closure, context);

        if (lhs.TryAs<runtime::Number>() && rhs.TryAs<runtime::Number>())
        {
            return ObjectHolder::Own(runtime::Number(lhs.TryAs<runtime::Number>()->GetValue()
                + rhs.TryAs<runtime::Number>()->GetValue()));
        }
        else if (lhs.TryAs<runtime::String>() && rhs.TryAs<runtime::String>())
        {
            return ObjectHolder::Own(runtime::String(lhs.TryAs<runtime::String>()->GetValue()
                + rhs.TryAs<runtime::String>()->GetValue()));
        }
        else if (lhs.TryAs<runtime::ClassInstance>() &&
            lhs.TryAs<runtime::ClassInstance>()->HasMethod(ADD_METHOD, 1))
        {
            return lhs.TryAs<runtime::ClassInstance>()->Call(ADD_METHOD, { rhs }, context);
        }
        else
        {
            throw std::runtime_error("Add: Error when adding two values."s);
        }
    }



    /***************   Sub   ***************/

    ObjectHolder Sub::Execute(Closure& closure, Context& context)
    {
        if (!lhs_ || !rhs_)
            throw std::runtime_error("Sub::Execute: Null pointer");

        ObjectHolder lhs = lhs_->Execute(closure, context);
        ObjectHolder rhs = rhs_->Execute(closure, context);

        if (lhs.TryAs<runtime::Number>() && rhs.TryAs<runtime::Number>())
        {
            return ObjectHolder::Own(runtime::Number(lhs.TryAs<runtime::Number>()->GetValue()
                - rhs.TryAs<runtime::Number>()->GetValue()));
        }
        else
        {
            throw std::runtime_error("Sub: Error when subtracting two values."s);
        }
    }



    /***************   Mult   ***************/

    ObjectHolder Mult::Execute(Closure& closure, Context& context)
    {
        if (!lhs_ || !rhs_)
            throw std::runtime_error("Mult::Execute: Null pointer");

        ObjectHolder lhs = lhs_->Execute(closure, context);
        ObjectHolder rhs = rhs_->Execute(closure, context);

        if (lhs.TryAs<runtime::Number>() && rhs.TryAs<runtime::Number>())
        {
            return ObjectHolder::Own(runtime::Number(lhs.TryAs<runtime::Number>()->GetValue()
                * rhs.TryAs<runtime::Number>()->GetValue()));
        }
        else
        {
            throw std::runtime_error("Mult: Error while multiplying two numbers."s);
        }
    }



    /***************   Div   ***************/

    ObjectHolder Div::Execute(Closure& closure, Context& context)
    {
        if (!lhs_ || !rhs_)
            throw std::runtime_error("Div::Execute: Null pointer");

        ObjectHolder lhs = lhs_->Execute(closure, context);
        ObjectHolder rhs = rhs_->Execute(closure, context);

        if (lhs.TryAs<runtime::Number>() && rhs.TryAs<runtime::Number>() &&
            (rhs.TryAs<runtime::Number>()->GetValue() != 0))
        {
            return ObjectHolder::Own(runtime::Number(lhs.TryAs<runtime::Number>()->GetValue()
                / rhs.TryAs<runtime::Number>()->GetValue()));
        }
        else
        {
            throw std::runtime_error("Div: Error when dividing two values."s);
        }
    }



    /***************   Compound   ***************/

    ObjectHolder Compound::Execute(Closure& closure, Context& context)
    {
        for (unique_ptr<Statement>& instruction : instructions_)
        {
            if (!instruction)
                throw std::runtime_error("Compound::Execute: Null pointer");
            instruction->Execute(closure, context);
        }
        return {};
    }



    /***************   Return   ***************/

    ObjectHolder Return::Execute(Closure& closure, Context& context)
    {
        if(expr_)
            throw ReturnValue(expr_->Execute(closure, context));
        return {};
    }



    /***************   ClassDefinition   ***************/

    ClassDefinition::ClassDefinition(ObjectHolder cls)
        : class_(std::move(cls))
    {
    }


    ObjectHolder ClassDefinition::Execute(Closure& closure, Context& /*context*/)
    {
        return closure.emplace(dynamic_cast<runtime::Class*>(class_.Get())->GetName(), class_).first->second;
    }



    /***************   FieldAssignment   ***************/

    FieldAssignment::FieldAssignment(VariableValue object, std::string field_name,
        std::unique_ptr<Statement> rv)
        : object_(std::move(object))
        , field_name_(std::move(field_name))
        , rv_(std::move(rv))
    {
    }


    ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context)
    {
        if (!rv_)
            throw std::runtime_error("FieldAssignment::Execute: Null pointer");
        runtime::ClassInstance* obj = dynamic_cast<runtime::ClassInstance*>(object_.Execute(closure, context).Get());
        obj->Fields()[field_name_] = rv_->Execute(closure, context);
        return obj->Fields().at(field_name_);
    }



    /***************   IfElse   ***************/

    IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
        std::unique_ptr<Statement> else_body)
        : condition_(std::move(condition))
        , if_body_(std::move(if_body))
        , else_body_(std::move(else_body))
    {
    }


    ObjectHolder IfElse::Execute(Closure& closure, Context& context)
    {
        if (!condition_)
            throw std::runtime_error("IfElse::Execute: Null pointer");
        ObjectHolder condition = condition_->Execute(closure, context);

        if (IsTrue(condition))
        {
            return if_body_->Execute(closure, context);
        }
        else if (else_body_)
        {
            return else_body_->Execute(closure, context);
        }
        else
        {
            return {};
        }
    }



    /***************   Or   ***************/

    ObjectHolder Or::Execute(Closure& closure, Context& context)
    {
        if (!lhs_)
            throw std::runtime_error("Or::Execute: Null pointer");
        ObjectHolder lhs = lhs_->Execute(closure, context);

        if (IsTrue(lhs))
            return ObjectHolder::Own(runtime::Bool(true));

        if (!rhs_)
            throw std::runtime_error("Or::Execute: Null pointer");
        ObjectHolder rhs = rhs_->Execute(closure, context);

        if (IsTrue(rhs))
            return ObjectHolder::Own(runtime::Bool(true));
        else
            return ObjectHolder::Own(runtime::Bool(false));
    }



    /***************   And   ***************/

    ObjectHolder And::Execute(Closure& closure, Context& context)
    {
        ObjectHolder lhs = lhs_->Execute(closure, context);

        if (!IsTrue(lhs))
            return ObjectHolder::Own(runtime::Bool(false));

        ObjectHolder rhs = rhs_->Execute(closure, context);
        if (IsTrue(rhs))
            return ObjectHolder::Own(runtime::Bool(true));
        else
            return ObjectHolder::Own(runtime::Bool(false));
    }



    /***************   Not   ***************/

    ObjectHolder Not::Execute(Closure& closure, Context& context)
    {
        if (!argument_)
            throw std::runtime_error("Not::Execute: Null pointer");

        ObjectHolder arg = argument_->Execute(closure, context);
        if (IsTrue(arg))
            return ObjectHolder::Own(runtime::Bool(false));
        else
            return ObjectHolder::Own(runtime::Bool(true));
    }



    /***************   Comparison   ***************/

    Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
        : BinaryOperation(std::move(lhs), std::move(rhs)), comparator_(cmp)
    {
    }


    ObjectHolder Comparison::Execute(Closure& closure, Context& context)
    {
        if (!lhs_ || !rhs_)
            throw std::runtime_error("Comparison::Execute: Null pointer");

        ObjectHolder lhs = lhs_->Execute(closure, context);
        ObjectHolder rhs = rhs_->Execute(closure, context);

        if (comparator_(lhs, rhs, context))
            return ObjectHolder::Own(runtime::Bool(true));
        else
            return ObjectHolder::Own(runtime::Bool(false));
    }



    /***************   NewInstance   ***************/

    NewInstance::NewInstance(const runtime::Class& cls)
        : cls_(cls)
    {
    }


    NewInstance::NewInstance(const runtime::Class& cls, std::vector<std::unique_ptr<Statement>> args)
        : cls_(cls), args_(std::move(args))
    {
    }


    ObjectHolder NewInstance::Execute(Closure& closure, Context& context)
    {
        auto holder = ObjectHolder::Own(runtime::ClassInstance{ cls_ });
        auto cls_inst = holder.TryAs<runtime::ClassInstance>();

        if (cls_inst->HasMethod(INIT_METHOD, args_.size()))
        {
            std::vector<runtime::ObjectHolder> actual_args;
            for (const auto& arg : args_)
            {
                actual_args.push_back(arg->Execute(closure, context));
            }
            cls_inst->Call(INIT_METHOD, actual_args, context);
        }
        return holder;
    }



    /***************   MethodBody   ***************/

    MethodBody::MethodBody(std::unique_ptr<Statement>&& body)
        : body_(std::move(body))
    {
    }


    ObjectHolder MethodBody::Execute(Closure& closure, Context& context)
    {
        try
        {
            if (!body_)
                throw  std::runtime_error("MethodBody::Execute: Null pointer");

            return body_->Execute(closure, context);
        }
        catch (ReturnValue& return_value)
        {
            return return_value;
        }
    }

}  // namespace ast