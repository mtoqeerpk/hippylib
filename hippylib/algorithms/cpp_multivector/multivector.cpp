/* Copyright (c) 2016-2018, The University of Texas at Austin
 * & University of California, Merced.
 *
 * All Rights reserved.
 * See file COPYRIGHT for details.
 *
 * This file is part of the hIPPYlib library. For more information and source
 * code availability see https://hippylib.github.io.
 *
 * hIPPYlib is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License (as published by the Free
 * Software Foundation) version 2.0 dated June 1991.
*/

#include "multivector.hpp"
#include <dolfin/la/PETScVector.h>

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

#include <cassert>

namespace py = pybind11;

namespace dolfin
{

MultiVector::MultiVector()
{
}

MultiVector::MultiVector(const GenericVector & v, int nvec):
		mv(nvec)
{
	for(auto&& vj : mv)
	{
		vj = v.copy();
		vj->zero();
	}
}

MultiVector::MultiVector(const MultiVector & orig):
		mv(orig.mv.size())
{
	int n = mv.size();
	for(int i = 0; i < n; ++i)
		mv[i] = orig.mv[i]->copy();
}

void MultiVector::setSizeFromVector(const GenericVector & v, int nvec)
{
	mv.resize(nvec);
	for(auto&& vj : mv)
	{
		vj = v.copy();
		vj->zero();
	}
}



std::shared_ptr<const GenericVector> MultiVector::operator[](int i) const
{
	return mv[i];
}

std::shared_ptr<GenericVector> MultiVector::operator[](int i)
{
	return mv[i];
}


void MultiVector::dot(const GenericVector & v, Array<double> & m) const
{
	double* im = m.data();
	for(auto&& vj : mv)
		*(im++) = vj->inner(v);
}

void MultiVector::dot(const MultiVector & other, Array<double> & m) const
{
	if(other.mv.begin() == mv.begin())
		dot_self(m);
	else
	{
		double* data = m.data();
		for(auto&& vi : mv)
			for(auto&& vj : other.mv)
				*(data++) = vi->inner(*vj);
	}
}

void MultiVector::dot_self(Array<double> & m) const
{
	int s = mv.size();
	for(int i = 0; i < s; ++i)
	{
		m[i + s*i] = mv[i]->inner(*(mv[i]));
		for(int j = 0; j < i; ++j)
			m[i + s*j] = m[j + s*i] = mv[i]->inner(*(mv[j]));

	}
}

void MultiVector::reduce(GenericVector & v, const Array<double> & alpha) const
{
	const double * data = alpha.data();
	for(auto&& vi : mv)
		v.axpy(*(data++), *vi);
}

void MultiVector::axpy(double a, const GenericVector & y)
{
	for(auto&& vi : mv)
		vi->axpy(a, y);
}

void MultiVector::axpy(const Array<double> & a, const MultiVector & y)
{
	int n = nvec();
	assert(a.size() == n);
	assert(y.nvec() == n);

	for(int i = 0; i < n; ++i)
		mv[i]->axpy(a[i], *(y.mv[i]) );
}

void MultiVector::scale(int k, double a)
{
	mv[k]->operator*=(a);
}

void MultiVector::scale(const Array<double> & a)
{
	const double * data = a.data();
	for(auto && vj : mv)
		vj->operator*=(*(data++));
}

void MultiVector::zero()
{
	for(auto&& vi : mv)
		vi->zero();
}

void MultiVector::norm_all(const std::string norm_type, Array<double> & norms) const
{
	double * data = norms.data();
	for(auto && vi : mv)
		*(data++) = vi->norm(norm_type);
}

void MultiVector::swap(MultiVector & other)
{
	mv.swap(other.mv);
}

MultiVector::~MultiVector()
{

}

}

PYBIND11_MODULE(SIGNATURE, m) {
    py::class_<dolfin::MultiVector>(m, "MultiVector")
    	.def(py::init<>())
		.def(py::init<const dolfin::GenericVector &, int>())
		.def(py::init<const dolfin::MultiVector &>())
		.def("nvec", &dolfin::MultiVector::nvec,
			 "Number of vectors in the multivector")
		.def("__len__", &dolfin::MultiVector::nvec,
			 "The length of a multivector is the number of vector it contains")
		.def("__getitem__", (std::shared_ptr<const dolfin::GenericVector> (dolfin::MultiVector::*)(int) const) &dolfin::MultiVector::operator[] )
		.def("__setitem__", (std::shared_ptr<dolfin::GenericVector> (dolfin::MultiVector::*)(int)) &dolfin::MultiVector::operator[] )
		.def("setSizeFromVector", &dolfin::MultiVector::setSizeFromVector,
			 "Initialize a multivector by providing a vector v as template and the number of vectors nvec",
			 py::arg("v"), py::arg("nvec"))
		.def("dot", [](const dolfin::MultiVector & self, const dolfin::GenericVector & v)
				{
    				int size = self.nvec();
    				py::array_t<double> ma(size);
    				dolfin::Array<double> m_dolfin(size, ma.mutable_data());
    				self.dot(v, m_dolfin);
    				return ma;
				},
				"Perform the inner product with a vector v",
				py::arg("v")
    	     )
		.def("dot", [](const dolfin::MultiVector& self, const dolfin::MultiVector & other)
				{
    				int size1 = self.nvec();
    				int size2 = other.nvec();
    				py::array_t<double> ma(size1*size2);
    				dolfin::Array<double> m_dolfin(size1*size2, ma.mutable_data());
    				self.dot(other, m_dolfin);
    				return ma;
				},
    			"Perform the inner product with a another multivector",
				py::arg("other")
			)
        .def("reduce", [](const dolfin::MultiVector& self, dolfin::GenericVector & v, py::array_t<double> & alpha)
        		{
    				dolfin::Array<double> alpha_dolfin(self.nvec(), alpha.mutable_data());
    				self.reduce(v, alpha_dolfin);
        		},
    		"Computes v += sum_i alpha[i]*self[i]",
			py::arg("v"), py::arg("alpha")
        )
		.def("axpy", (void (dolfin::MultiVector::*)(double, const dolfin::GenericVector &)) &dolfin::MultiVector::axpy,
			 "Assign self[k] += a*y for k in range(self.nvec())",
			 py::arg("a"), py::arg("y"))
		.def("axpy", [](dolfin::MultiVector& self, py::array_t<double> & a, const dolfin::MultiVector& y)
				{
    				dolfin::Array<double> a_dolfin(self.nvec(), a.mutable_data());
    				self.axpy(a_dolfin, y);
				},
				"Assign self[k] += a[k]*y[k] for k in range(self.nvec())",
				py::arg("a"), py::arg("y")
				)
		.def("scale", (void (dolfin::MultiVector::*)(int, double)) &dolfin::MultiVector::scale,
			 "Assign self[k] *= a",
			 py::arg("k"), py::arg("a"))
		.def("scale", [](dolfin::MultiVector& self, py::array_t<double> & a)
				{
					dolfin::Array<double> a_dolfin(self.nvec(), a.mutable_data());
					self.scale(a_dolfin);
				},
				"Assign self[k] *=a[k] for k in range(self.nvec()",
				py::arg("a")
				)
		.def("zero",  &dolfin::MultiVector::zero,
			 "Zero out all entries of the multivector"
			)
		.def("norm",[](const dolfin::MultiVector& self, const std::string norm_type)
				{
					int size = self.nvec();
					py::array_t<double> ma(size);
					dolfin::Array<double> m_dolfin(size, ma.mutable_data());
					self.norm_all(norm_type, m_dolfin);
					return ma;
				},
			 "Compute the norm of each vector in the multivector separately",
			 py::arg("norm_type")
			)
		.def("swap", &dolfin::MultiVector::swap,
			 "Swap this with other");
}
