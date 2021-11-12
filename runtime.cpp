#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>

using namespace std;

namespace runtime
{

    /*****************************************************
    **************   Class ObjectHolder   ***************
    ******************************************************/

    ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
        : data_(std::move(data))
    {
    }

    void ObjectHolder::AssertIsValid() const
    {
        assert(data_ != nullptr);
    }

    ObjectHolder ObjectHolder::Share(Object& object)
    {
        // Возвращаем невладеющий shared_ptr (его deleter ничего не делает)
        return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
    }


    ObjectHolder ObjectHolder::None()
    {
        return ObjectHolder();
    }


    Object& ObjectHolder::operator*() const
    {
        AssertIsValid();
        return *Get();
    }


    Object* ObjectHolder::operator->() const
    {
        AssertIsValid();
        return Get();
    }


    Object* ObjectHolder::Get() const
    {
        return data_.get();
    }


    ObjectHolder::operator bool() const
    {
        return Get() != nullptr;
    }



    bool IsTrue(const ObjectHolder& object)
    {
        if (object)
        {
            if (object.TryAs<String>())
            {
                return !object.TryAs<String>()->GetValue().empty();
            }
            else if (object.TryAs<Bool>())
            {
                return object.TryAs<Bool>()->GetValue();
            }
            else if (object.TryAs<Number>())
            {
                return object.TryAs<Number>()->GetValue() != 0;
            }
            else
            {
                return false;
            }
        }
        else
        {
            return false;

        }
    }



    /*****************************************************
    **************   Class ClassInstance   ***************
    ******************************************************/

    ClassInstance::ClassInstance(const Class& cls)
        : class_(cls)
        , class_field_()
    {
    }


    void ClassInstance::Print(std::ostream& os, Context& context)
    {
        if (HasMethod("__str__"s, 0))
            Call("__str__"s, {}, context)->Print(os, context);
        else
            os << this;
    }


    bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const
    {
        return class_.GetMethod(method) != nullptr &&
            (class_.GetMethod(method)->formal_params.size()) == argument_count;
    }


    Closure& ClassInstance::Fields()
    {
        return class_field_;
    }


    const Closure& ClassInstance::Fields() const
    {
        return const_cast<const Closure&>(class_field_);
    }


    ObjectHolder ClassInstance::Call(const std::string& method,
        const std::vector<ObjectHolder>& actual_args,
        Context& context)
    {
        if (HasMethod(method, actual_args.size()))
        {
            // Создаём контейнер для аргументов, передающихся в метод
            Closure args;

            // Передаём в метод ссылку на обьект (self)
            args.emplace("self"s, ObjectHolder::Share(*this));

            for (size_t i = 0; i < actual_args.size(); i++)
            {
                // Заполняем контейнер аргументов: передаём имя параметра, взятое из метода и аргумент, переданный в метод
                args.emplace(class_.GetMethod(method)->formal_params[i], actual_args[i]);
            }

            // Вызываем у метода тело с переданными аргументами 
            return class_.GetMethod(method)->body->Execute(args, context);
        }
        else
        {
            throw std::runtime_error("ClassInstance::Call: No method with passed parameters"s);
        }
    }



    /*****************************************************
    ******************   Class Class   *******************
    ******************************************************/

    Class::Class(std::string name, std::vector<Method> methods, const Class* parent)
        : methods_()
        , name_(name)
        , parent_(parent)
    {
        for (Method& method : methods)
            methods_[method.name] = std::move(method);
    }

    const Method* Class::GetMethod(const std::string& name) const
    {
        if (methods_.count(name))
        {
            return &methods_.at(name);
        }
        else if (parent_ != nullptr && parent_->GetMethod(name))
        {
            return parent_->GetMethod(name);
        }
        else
        {
            return nullptr;
        }
    }

    [[nodiscard]] const std::string& Class::GetName() const
    {
        return const_cast<std::string&>(name_);
    }

    void Class::Print(ostream& os, Context& /*context*/)
    {
        os << "Class " << name_;
    }



    /*****************************************************
    *****************   Class String   *******************
    ******************************************************/

    void String::Print(std::ostream& os, [[maybe_unused]] Context& context)
    {
        os << GetValue();
    }



    /*****************************************************
    *****************   Class Number   *******************
    ******************************************************/

    void Number::Print(std::ostream& os, [[maybe_unused]] Context& context)
    {
        os << GetValue();
    }



    /*****************************************************
    ******************   Class Bool   ********************
    ******************************************************/

    void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context)
    {
        os << (GetValue() ? "True"sv : "False"sv);
    }



    /*****************************************************
    *******************   Supp Func   ********************
    ******************************************************/

    bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context)
    {
        const auto cls_obj = lhs.TryAs<ClassInstance>();
        if (cls_obj != nullptr && cls_obj->HasMethod("__eq__", 1))
            return cls_obj->Call("__eq__", { rhs }, context).TryAs<Bool>()->GetValue();

        if (lhs.TryAs<String>() && rhs.TryAs<String>())
        {
            return lhs.TryAs<String>()->GetValue() == rhs.TryAs<String>()->GetValue();
        }
        else if (lhs.TryAs<Bool>() && rhs.TryAs<Bool>())
        {
            return lhs.TryAs<Bool>()->GetValue() == rhs.TryAs<Bool>()->GetValue();
        }
        else if (lhs.TryAs<Number>() && rhs.TryAs<Number>())
        {
            return lhs.TryAs<Number>()->GetValue() == rhs.TryAs<Number>()->GetValue();
        }
        else if (!lhs && !rhs)
        {
            return true;
        }

        throw std::runtime_error("Equal: No implementation of comparing two passed objects"s);
    }


    bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context)
    {
        ClassInstance* cls_obj = lhs.TryAs<ClassInstance>();
        if (cls_obj != nullptr && cls_obj->HasMethod("__lt__", 1))
            return cls_obj->Call("__lt__", { rhs }, context).TryAs<Bool>()->GetValue();

        if (lhs.TryAs<String>() && rhs.TryAs<String>())
        {
            return lhs.TryAs<String>()->GetValue() < rhs.TryAs<String>()->GetValue();
        }
        else if (lhs.TryAs<Bool>() && rhs.TryAs<Bool>())
        {
            return lhs.TryAs<Bool>()->GetValue() < rhs.TryAs<Bool>()->GetValue();
        }
        else if (lhs.TryAs<Number>() && rhs.TryAs<Number>())
        {
            return lhs.TryAs<Number>()->GetValue() < rhs.TryAs<Number>()->GetValue();
        }

        throw std::runtime_error("Less: No implementation of comparing two passed objects"s);
    }


    bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context)
    {
        return !Equal(lhs, rhs, context);
    }


    bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context)
    {
        return !Less(lhs, rhs, context) && NotEqual(lhs, rhs, context);
    }


    bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context)
    {
        return !Greater(lhs, rhs, context);
    }


    bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context)
    {
        return !Less(lhs, rhs, context);
    }

}  // namespace runtime